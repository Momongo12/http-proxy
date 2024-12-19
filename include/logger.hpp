#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>

class Logger {
public:
    static void info(const std::string &msg);
    static void error(const std::string &msg);
};

#endif // LOGGER_HPP
