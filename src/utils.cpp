#include "utils.hpp"
#include <algorithm>

std::string Utils::trim(const std::string &s) {
    auto start = s.begin();
    while (start != s.end() && isspace(static_cast<unsigned char>(*start))) start++;
    auto end = s.end();
    do { end--; } while (end != start && isspace(static_cast<unsigned char>(*end)));
    return std::string(start, end + 1);
}
