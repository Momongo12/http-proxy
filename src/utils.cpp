#include "utils.hpp"
#include <algorithm>

std::string Utils::trim(const std::string &s) {
    if (s.empty()) return s;

    size_t start = 0;
    while (start < s.size() && std::isspace((unsigned char)s[start])) {
        start++;
    }
    if (start == s.size()) {
        // Все символы пробельные
        return "";
    }
    size_t end = s.size() - 1;
    while (end > start && std::isspace((unsigned char)s[end])) {
        end--;
    }
    return s.substr(start, end - start + 1);
}
