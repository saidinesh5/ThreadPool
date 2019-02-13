ThreadPool
==========

A simple C++11 Thread Pool implementation.

Basic usage:
```c++
// create thread pool with 4 worker threads
ThreadPool pool(4);

// enqueue and store future
auto result = pool.enqueue([](int answer) { return answer; }, 42);

// get result from future
std::cout << result.get() << std::endl;

//Alternatively you can use the ThreadPool singleton instance, because why not.
ThreadPool::instance()->enqueue([](int answer){ return answer;}, 42);

//Prioritize tasks
pool.enqueue(ThreadPool::LowPriority,
             [](int answer){ return answer; },
             1);

pool.enqueue(ThreadPool::HighPriority,
             [](int answer){ return answer; },
             2);

//Wait until pool is done.
if (pool.pendingTask() > 0)
    pool.wait();
```
