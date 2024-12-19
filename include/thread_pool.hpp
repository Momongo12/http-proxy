#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>

class ThreadPool {
public:
    ThreadPool() = default;
    ~ThreadPool();
    bool init(int numThreads);
    void submitTask(int clientFd);
    void shutdown();

private:
    void workerFunc();
    std::vector<std::thread> workers;
    std::queue<int> tasks;
    std::mutex mtx;
    std::condition_variable cv;
    std::atomic<bool> stop{false};
};

#endif // THREAD_POOL_HPP
