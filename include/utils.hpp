#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>

namespace Utils {
    std::string trim(const std::string &s);

    bool parseUrl(const std::string &url, std::string &scheme, std::string &host, int &port, std::string &path);
}

#endif // UTILS_HPP
