#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <condition_variable>
#include <functional>
#include <future>
#include <limits>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <vector>
#include <atomic>

class ThreadPool
{
    std::atomic_bool mStop;
    // need to keep track of threads so we can join them
    std::vector<std::thread> mWorkers;

    using prioritized_task = std::tuple<int, std::function<void()>>;
    std::priority_queue<prioritized_task,
                        std::vector<prioritized_task>,
                        std::function<bool(const prioritized_task&, const prioritized_task&)>> mTasks;

    // synchronization
    std::mutex mQueueMutex;
    std::mutex mTaskFinishedMutex;
    std::condition_variable mQueueUpdatedCondition;
    std::condition_variable mTaskFinishedCondition;

public:

    enum Priority
    {
        LowPriority = std::numeric_limits<int16_t>::min(),
        NormalPriority = 0,
        HighPriority = std::numeric_limits<int16_t>::max()
    };

    /**
     * @brief ThreadPool constructor - by default sets the concurrency to the number of processor threads
     * @param threads
     */
    explicit ThreadPool(size_t threads = std::thread::hardware_concurrency()):
        mStop{false},
        mWorkers{},
        mTasks{[](prioritized_task const &l, prioritized_task const &r) { return std::get<0>(l) < std::get<0>(r); }},
        mQueueMutex{},
        mTaskFinishedMutex{},
        mQueueUpdatedCondition{},
        mTaskFinishedCondition{}
    {
        for(size_t i = 0; i < threads; i++)
        {
            mWorkers.emplace_back([this]()
                                  {
                                      for(;;)
                                      {
                                          std::function<void()> task;

                                          {
                                              std::unique_lock<std::mutex> lock(this->mQueueMutex);
                                              this->mQueueUpdatedCondition.wait(lock,
                                                                                [this](){ return this->mStop.load() || !this->mTasks.empty();});

                                              //End the worker thread immediately if it is asked to stop
                                              if(this->mStop.load())
                                              {
                                                  return;
                                              }
                                              else
                                              {
                                                  task = std::get<1>(std::move(this->mTasks.top()));
                                                  this->mTasks.pop();
                                              }
                                          }

                                          task();
                                          mTaskFinishedCondition.notify_all();
                                      }
                                  });
        }
    }

    /**
     * @brief ThreadPool destructor - tries to stop and wait for all the threads before quitting
     */
    ~ThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock(mQueueMutex);
            mStop.store(true);
        }

        mQueueUpdatedCondition.notify_all();
        for(std::thread &worker: mWorkers)
        {
            worker.join();
        }
    }

    /**
     * @brief enqueue a task onto the threadpool
     * @param priority priority of the task to be enqueued. You can use LowPriority, NormalPriority, HighPriority for this.
     * @param f - function to be enqueued to the threadpool
     * @param args - args to the function that is enqueued
     * eg. std::future<int> result = ThreadPool::instance()->enqueue(ThreadPool::NormalPriority,
     *                                                               [](){ std::this_thread::sleep_for(std::chrono::seconds(i)); return 5*5;  });
     */
    template<class F, class... Args>
    std::future<typename std::result_of<F(Args...)>::type> enqueue(int priority,
                                                                   F&& f,
                                                                   Args&&... args)
    {
        using return_type = typename std::result_of<F(Args...)>::type;
        auto task = std::make_shared<std::packaged_task<return_type()>>(std::bind(std::forward<F>(f),
                                                                                  std::forward<Args>(args)...));

        std::future<return_type> res = task->get_future();

        {
            std::unique_lock<std::mutex> lock(mQueueMutex);

            // don't allow enqueueing after stopping the pool
            if(mStop.load())
            {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }

            mTasks.emplace(prioritized_task(priority,
                                            [task](){ (*task)(); }));
        }

        mQueueUpdatedCondition.notify_one();
        return res;
    }

    /**
     * @brief enqueue a task onto the threadpool with Normal Priority
     * @param f - function to be enqueued to the threadpool
     * @param args - args to the function that is enqueued
     * eg. std::future<int> result = ThreadPool::instance()->enqueue([](){ std::this_thread::sleep_for(std::chrono::seconds(i)); return 5*5;  });
     */
    template<class F, class... Args>
    std::future<typename std::result_of<F(Args...)>::type> enqueue(F&& f,
                                                                   Args&&... args)
    {
        return enqueue(static_cast<int>(NormalPriority),
                       f,
                       args...);
    }

    /**
     * @brief wait - wait until all the tasks have been processed
     */
    void wait()
    {
        for(const auto &worker: mWorkers)
        {
            if (std::this_thread::get_id() == worker.get_id())
            {
                throw std::runtime_error("Cannot wait on threadpool from within a task");
            }
        }

        while(!mTasks.empty())
        {
            std::unique_lock<std::mutex> lock(this->mTaskFinishedMutex);
            this->mTaskFinishedCondition.wait(lock,
                                              [this]() { return this->mTasks.empty(); });
        }
    }

    size_t pendingTasks() const
    {
        return mTasks.size();
    }

    /**
     * @brief instance of the Singleton Threadpool, For Application-wide usage
     * @return
     */
    static ThreadPool* instance()
    {
        static ThreadPool p;
        return &p;
    }
};

#endif
