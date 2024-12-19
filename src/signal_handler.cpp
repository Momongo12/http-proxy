#include "signal_handler.hpp"
#include <csignal>

std::atomic<bool> SignalHandler::shutdownRequested{false};

void SignalHandler::handleSignal(int) {
    shutdownRequested.store(true, std::memory_order_relaxed);
}

void SignalHandler::init() {
    struct sigaction sa;
    sa.sa_handler = handleSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
}

bool SignalHandler::shouldShutdown() {
    return shutdownRequested.load(std::memory_order_relaxed);
}
