#ifndef SIGNAL_HANDLER_HPP
#define SIGNAL_HANDLER_HPP

#include <atomic>

class SignalHandler {
public:
    static void init();
    static bool shouldShutdown();
private:
    static void handleSignal(int signum);
    static std::atomic<bool> shutdownRequested;
};

#endif // SIGNAL_HANDLER_HPP
