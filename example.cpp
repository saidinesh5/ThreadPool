#include <iostream>
#include <vector>
#include <chrono>
#include <random>

#include "ThreadPool.h"

int main()
{
    std::vector< std::future<int> > results;
    const int tasks = 8;

    //We use a random sleep timer to show the concurrency of this threadpool
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, tasks);

    //Just normal usage
    {
        ThreadPool pool;
        for(int i = 0; i < tasks; ++i)
        {
            int sleeptime = dis(gen)*100;

            results.emplace_back(pool.enqueue([i, sleeptime]()
                                              {
                                                  std::cout << "Task " << i << " will now sleep for " << sleeptime << " ms." << std::endl;
                                                  std::this_thread::sleep_for(std::chrono::milliseconds(sleeptime));
                                                  return i*i;
                                              }));
        }

        pool.wait();
    }

    for(size_t i = 0; i < tasks; i++)
    {
        std::cout << "Result of task: " << i << ": " << results[i].get() << ' ' << std::endl;
    }

    //Testing the prioritized tasks
    //We use a threadpool with 1 worker thread, to make sure all the tasks are funneled through that.
    //Then irrespective of what order they are enqueued in, they will always be executed in the order of highest priority first
    ThreadPool prioritizedPool(1);
    std::vector<int> taskIds(tasks, -1);
    std::atomic_int currentTask{0};

    //Make the thread pool wait until all the tasks have been enqueued
    prioritizedPool.enqueue([](){ std::this_thread::sleep_for(std::chrono::seconds(2)); });

    for(int i = 0; i < tasks; i++)
    {
        prioritizedPool.enqueue(i, //Priority. Initial tasks have the least priority
                                [i, &taskIds, &currentTask]()
                                {
                                    taskIds[static_cast<size_t>(currentTask++)] = i;
                                });
    }

    prioritizedPool.wait();

    std::cout << "The order of execution of the tasks is:" <<std::endl;

    for(const auto& taskId: taskIds)
    {
        std::cout << taskId << " ";
    }

    std::cout << std::endl;

    return 0;
}
