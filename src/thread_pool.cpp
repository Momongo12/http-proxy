#include "thread_pool.hpp"
#include "logger.hpp"
#include "http_parser.hpp"
#include <unistd.h>

bool ThreadPool::init(int numThreads) {
    for (int i = 0; i < numThreads; i++) {
        workers.emplace_back(&ThreadPool::workerFunc, this);
    }
    return true;
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mtx);
        stop.store(true, std::memory_order_relaxed);
    }
    cv.notify_all();
    for (auto &w : workers) {
        if (w.joinable()) w.join();
    }
}

void ThreadPool::submitTask(int clientFd) {
    {
        std::lock_guard<std::mutex> lock(mtx);
        tasks.push(clientFd);
    }
    cv.notify_one();
}

void ThreadPool::workerFunc() {
    while (true) {
        int clientFd;
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [this] { return stop.load(std::memory_order_relaxed) || !tasks.empty(); });
            if (stop.load(std::memory_order_relaxed) && tasks.empty()) break;
            clientFd = tasks.front();
            tasks.pop();
        }

        std::string buffer;
        char buf[1024];
        ssize_t n = read(clientFd, buf, sizeof(buf)-1);
        if (n > 0) {
            buf[n] = '\0';
            buffer = buf;
        } else if (n == 0) {
            // Клиент закрыл соединение, запрос может быть пустым
            // Можно просто закрыть клиент и продолжить
            close(clientFd);
            continue;
        } else {
            Logger::error("Error reading from client");
            close(clientFd);
            continue;
        }

        HttpRequest req;
        HttpParser parser;
        if (parser.parse(buffer, req)) {
            Logger::info("Parsed request: method=" + req.method + " path=" + req.path + " version=" + req.version);
            if (req.headers.find("host") != req.headers.end()) {
                Logger::info("Host: " + req.headers["host"]);
            }
        } else {
            Logger::error("Failed to parse HTTP request");
        }

        close(clientFd);
    }
}

