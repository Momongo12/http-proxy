#include "thread_pool.hpp"
#include "logger.hpp"
#include "http_parser.hpp"
#include "connection_handler.hpp"
#include <unistd.h>
#include <sys/socket.h>

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

        Logger::info("ThreadPool: Handling new client fd=" + std::to_string(clientFd));

        std::string buffer;
        char buf[1024];
        ssize_t n = read(clientFd, buf, sizeof(buf)-1);
        if (n <= 0) {
            Logger::error("ThreadPool: client closed connection or read error");
            close(clientFd);
            continue;
        }
        buf[n] = '\0';
        buffer = buf;

        HttpRequest req;
        HttpParser parser;
        if (!parser.parse(buffer, req)) {
            Logger::error("ThreadPool: Failed to parse HTTP request");
            std::string err = "HTTP/1.0 400 Bad Request\r\n\r\nBad Request\r\n";
            send(clientFd, err.data(), err.size(), 0);
            close(clientFd);
            continue;
        }

        if (req.method != "GET") {
            Logger::info("ThreadPool: Request method not implemented: " + req.method);
            std::string err = "HTTP/1.0 501 Not Implemented\r\n\r\nMethod Not Implemented\r\n";
            send(clientFd, err.data(), err.size(), 0);
            close(clientFd);
            continue;
        }

        Logger::info("ThreadPool: Parsed request: " + req.method + " " + req.path + " " + req.version);
        auto h = req.headers.find("host");
        if (h != req.headers.end()) {
            Logger::info("ThreadPool: Host: " + h->second);
        }

        ConnectionHandler handler;
        if (!handler.processRequest(req, clientFd)) {
            Logger::error("ThreadPool: processRequest failed, sending error to client");
            std::string err = "HTTP/1.0 502 Bad Gateway\r\n\r\nError processing request\r\n";
            send(clientFd, err.data(), err.size(), 0);
        }

        // В этот момент данные уже отправлены клиенту внутри processRequest,
        // либо в случае ошибки мы отправили сообщение об ошибке.

        close(clientFd);
        Logger::info("ThreadPool: Finished handling client");
    }
}
