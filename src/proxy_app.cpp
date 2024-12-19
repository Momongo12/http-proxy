#include "proxy_app.hpp"
#include "logger.hpp"
#include "signal_handler.hpp"
#include <getopt.h>
#include <iostream>

void ProxyApp::parseArgs(int argc, char** argv) {
    static struct option long_options[] = {
            {"help", no_argument, nullptr, 'h'},
            {"port", required_argument, nullptr, 'p'},
            {"max-client-threads", required_argument, nullptr, 'm'},
            {nullptr, 0, nullptr, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "hp:m:", long_options, nullptr)) != -1) {
        switch(opt) {
            case 'h':
                helpFlag = true;
                break;
            case 'p':
                config.port = std::stoi(optarg);
                break;
            case 'm':
                config.maxThreads = std::stoi(optarg);
                break;
            default:
                std::cerr << "Unknown option or missing argument. Use --help for usage.\n";
                exit(1);
        }
    }
}

void ProxyApp::init() {
    SignalHandler::init();
    if (!listener.startListening(config.port)) {
        Logger::error("Cannot start listener");
        exit(1);
    }
    if (!pool.init(config.maxThreads)) {
        Logger::error("Cannot init thread pool");
        exit(1);
    }
    Logger::info("Initialized with port=" + std::to_string(config.port) + " threads=" + std::to_string(config.maxThreads));
}

void ProxyApp::run() {
    int listenFd = listener.getSocketFd();
    while(!SignalHandler::shouldShutdown()) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(listenFd, &readfds);
        int maxfd = listenFd;

        int ret = select(maxfd+1, &readfds, nullptr, nullptr, nullptr);
        if (ret < 0) {
            if (SignalHandler::shouldShutdown()) break;
            Logger::error("select() failed");
            break;
        }

        if (FD_ISSET(listenFd, &readfds)) {
            int clientFd = listener.acceptClient();
            if (clientFd >= 0) {
                Logger::info("Accepted new client: fd=" + std::to_string(clientFd));
                pool.submitTask(clientFd);
            }
        }
    }
}

void ProxyApp::shutdown() {
    Logger::info("Received shutdown signal");
    pool.shutdown();
    Logger::info("All threads have finished");
    Logger::info("Proxy finished");
}

void ProxyApp::printHelp() {
    std::cout << "Usage: http_proxy [--port N] [--max-client-threads N] [--help]\n";
}
