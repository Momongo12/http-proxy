#ifndef HTTP_PARSER_HPP
#define HTTP_PARSER_HPP

#include <string>
#include <unordered_map>

struct HttpRequest {
    std::string method;
    std::string path;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
};

class HttpParser {
public:
    // Возвращает true, если запрос успешно разобран
    bool parse(const std::string &data, HttpRequest &request);
private:
    bool parseStartLine(const std::string &line, HttpRequest &req);
    std::string toLower(const std::string &s);
};

#endif // HTTP_PARSER_HPP
