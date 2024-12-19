#include "logger.hpp"
#include <iostream>
#include <mutex>

static std::mutex logMutex;

void Logger::info(const std::string &msg) {
    std::lock_guard<std::mutex> lock(logMutex);
    std::cout << "[INFO] " << msg << "\n";
}

void Logger::error(const std::string &msg) {
    std::lock_guard<std::mutex> lock(logMutex);
    std::cerr << "[ERROR] " << msg << "\n";
}