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

bool Utils::parseUrl(const std::string &url, std::string &scheme, std::string &host, int &port, std::string &path) {
    port = 80;
    scheme.clear();
    host.clear();
    path = "/";

    std::string u = trim(url);
    if (u.empty()) return false;

    // Ищем "://"
    auto schemePos = u.find("://");
    if (schemePos != std::string::npos) {
        scheme = u.substr(0, schemePos);
        u = u.substr(schemePos + 3); // пропускаем "://"
    } else {
        // Если нет схемы, считаем, что схема http
        scheme = "http";
    }

    if (scheme == "https") {
        // Упростим: считаем https => порт 80, но в реальности нужно https.
        port = 80;
    } else if (scheme != "http") {
        // Неизвестная схема
        return false;
    }

    // Теперь в u: host[:port]/path или просто host или host/path
    // Находим первый '/'
    auto slashPos = u.find('/');
    std::string hostPort = (slashPos == std::string::npos) ? u : u.substr(0, slashPos);
    if (slashPos != std::string::npos && slashPos+1 <= u.size()) {
        path = u.substr(slashPos);
        if (path.empty()) path = "/";
    }

    // Разбираем hostPort на host[:port]
    auto colonPos = hostPort.find(':');
    if (colonPos != std::string::npos) {
        host = trim(hostPort.substr(0, colonPos));
        std::string portStr = trim(hostPort.substr(colonPos+1));
        if (portStr.empty()) return false;
        try {
            port = std::stoi(portStr);
        } catch (...) {
            return false;
        }
    } else {
        host = trim(hostPort);
    }

    if (host.empty() || port <= 0) return false;
    return true;
}
