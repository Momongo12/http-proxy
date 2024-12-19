#ifndef PROXY_APP_HPP
#define PROXY_APP_HPP

#include "config.hpp"
#include "listener.hpp"
#include "thread_pool.hpp"

class ProxyApp {
public:
    ProxyApp() = default;
    void parseArgs(int argc, char** argv);
    void init();
    void run();
    void shutdown();
    bool showHelp() const { return helpFlag; }
    void printHelp();
private:
    Config config;
    bool helpFlag = false;
    Listener listener;
    ThreadPool pool;
};

#endif // PROXY_APP_HPP
