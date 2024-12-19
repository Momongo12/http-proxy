#include "http_parser.hpp"
#include "utils.hpp"
#include <sstream>

bool HttpParser::parse(const std::string &data, HttpRequest &request) {
    std::istringstream iss(data);
    std::string line;

    // Первая строка — стартовая
    if (!std::getline(iss, line)) return false;
    line = Utils::trim(line);
    if (!parseStartLine(line, request)) return false;

    // Заголовки
    while (std::getline(iss, line)) {
        line = Utils::trim(line);
        if (line.empty()) {
            // Пустая строка - конец заголовков
            break;
        }
        auto pos = line.find(':');
        if (pos == std::string::npos) continue;
        std::string name = Utils::trim(line.substr(0, pos));
        std::string value = Utils::trim(line.substr(pos+1));
        request.headers[toLower(name)] = value;
    }

    return true;
}

bool HttpParser::parseStartLine(const std::string &line, HttpRequest &req) {
    std::istringstream iss(line);
    if (!(iss >> req.method >> req.path >> req.version)) {
        return false;
    }
    return true;
}

std::string HttpParser::toLower(const std::string &s) {
    std::string out;
    out.reserve(s.size());
    for (auto c : s) out.push_back(static_cast<char>(tolower(static_cast<unsigned char>(c))));
    return out;
}
