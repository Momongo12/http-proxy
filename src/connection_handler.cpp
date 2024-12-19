#include "connection_handler.hpp"
#include "logger.hpp"
#include "utils.hpp"
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sstream>
#include <string.h>

bool ConnectionHandler::processRequest(const HttpRequest &req, int clientFd) {
    Logger::info("ConnectionHandler: processing request: " + req.method + " " + req.path);

    std::string host;
    int port;
    std::string path;
    if (!parseFinalUrl(req, host, port, path)) {
        Logger::error("ConnectionHandler: Could not parse final URL from request");
        std::string err = "HTTP/1.0 400 Bad Request\r\n\r\nInvalid URL.\r\n";
        send(clientFd, err.data(), err.size(), 0);
        return false;
    }

    HttpRequest actualReq = req;
    actualReq.path = path;
    actualReq.headers["host"] = (port != 80) ? (host + ":" + std::to_string(port)) : host;

    int serverFd = connectToServer(host, port);
    if (serverFd < 0) {
        Logger::error("ConnectionHandler: Could not connect to " + host + ":" + std::to_string(port));
        std::string err = "HTTP/1.0 502 Bad Gateway\r\n\r\nCould not connect to upstream server.\r\n";
        send(clientFd, err.data(), err.size(), 0);
        return false;
    }

    if (!sendRequest(serverFd, actualReq)) {
        Logger::error("ConnectionHandler: Failed to send request to server");
        close(serverFd);
        std::string err = "HTTP/1.0 502 Bad Gateway\r\n\r\nFailed to send request.\r\n";
        send(clientFd, err.data(), err.size(), 0);
        return false;
    }

    std::string location;
    int redirectCount = 0;
    bool done = false;

    // Читаем заголовки и проверяем на редирект.
    // Если редирект - повторяем запрос.
    while (!done) {
        if (!readHeadersAndCheckRedirect(serverFd, clientFd, location)) {
            // Если мы здесь, значит ошибка уже отправлена клиенту.
            close(serverFd);
            return false;
        }

        if (!location.empty()) {
            close(serverFd);
            redirectCount++;
            if (redirectCount > 5) {
                Logger::error("ConnectionHandler: too many redirects");
                // Заголовки редиректа уже отправлены клиенту, он сам решит, что делать
                return true;
            }

            std::string newHost;
            int newPort;
            std::string newPath;
            if (!parseRedirectUrl(location, newHost, newPort, newPath)) {
                Logger::error("ConnectionHandler: invalid redirect location: " + location);
                return true;
            }

            serverFd = connectToServer(newHost, newPort);
            if (serverFd < 0) {
                Logger::error("ConnectionHandler: Could not connect to redirect location: " + newHost + ":" + std::to_string(newPort));
                return true;
            }

            HttpRequest newReq = req;
            newReq.path = newPath;
            newReq.headers["host"] = (newPort != 80) ? (newHost + ":" + std::to_string(newPort)) : newHost;

            if (!sendRequest(serverFd, newReq)) {
                Logger::error("ConnectionHandler: Failed to send request after redirect");
                close(serverFd);
                return true;
            }

            location.clear();
            // Переходим к следующей итерации цикла, чтобы прочитать новые заголовки
        } else {
            // Не редирект, значит дальше тело ответа
            if (!streamResponse(serverFd, clientFd)) {
                Logger::error("ConnectionHandler: Error streaming response body");
            }
            close(serverFd);
            done = true;
        }
    }

    return true;
}

bool ConnectionHandler::parseFinalUrl(const HttpRequest &req, std::string &host, int &port, std::string &path) {
    // Если путь полный URL
    if (req.path.find("http://") == 0 || req.path.find("https://") == 0) {
        std::string scheme;
        return Utils::parseUrl(req.path, scheme, host, port, path);
    } else {
        // Собираем URL из Host и path
        auto it = req.headers.find("host");
        if (it == req.headers.end()) return false;
        std::string fullUrl = "http://" + Utils::trim(it->second) + req.path;
        std::string scheme;
        return Utils::parseUrl(fullUrl, scheme, host, port, path);
    }
}

bool ConnectionHandler::parseRedirectUrl(const std::string &location, std::string &host, int &port, std::string &path) {
    std::string scheme;
    return Utils::parseUrl(location, scheme, host, port, path);
}

int ConnectionHandler::connectToServer(const std::string &host, int port) {
    Logger::info("ConnectionHandler: connecting to " + host + ":" + std::to_string(port));
    struct addrinfo hints, *res;
    ::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    std::string portStr = std::to_string(port);
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0) {
        Logger::error("ConnectionHandler: getaddrinfo failed for " + host);
        return -1;
    }
    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
        freeaddrinfo(res);
        Logger::error("ConnectionHandler: socket creation failed for " + host);
        return -1;
    }
    if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        Logger::error("ConnectionHandler: connect failed for " + host);
        close(fd);
        freeaddrinfo(res);
        return -1;
    }
    freeaddrinfo(res);
    return fd;
}

bool ConnectionHandler::sendRequest(int serverFd, const HttpRequest &req) {
    Logger::info("ConnectionHandler: sending request to server: " + req.method + " " + req.path);
    std::ostringstream oss;
    oss << req.method << " " << req.path << " HTTP/1.0\r\n";
    for (auto &h : req.headers) {
        oss << h.first << ": " << h.second << "\r\n";
    }
    oss << "\r\n";

    std::string out = oss.str();
    ssize_t written = send(serverFd, out.data(), out.size(), 0);
    if (written != (ssize_t)out.size()) {
        Logger::error("ConnectionHandler: sendRequest incomplete");
        return false;
    }
    return true;
}

bool ConnectionHandler::readHeadersAndCheckRedirect(int serverFd, int clientFd, std::string &location) {
    location.clear();
    std::string headers;
    char c;
    bool headersDone = false;

    // Читаем заголовки до \r\n\r\n
    while (!headersDone) {
        ssize_t r = recv(serverFd, &c, 1, 0);
        if (r <= 0) {
            // Сервер закрыл соединение или ошибка
            std::string err = "HTTP/1.0 502 Bad Gateway\r\n\r\nEmpty or invalid response\r\n";
            send(clientFd, err.data(), err.size(), 0);
            return false;
        }
        headers.push_back(c);
        int len = (int)headers.size();
        if (len >= 4 && headers.substr(len-4) == "\r\n\r\n") {
            headersDone = true;
        }
    }

    if (headers.empty()) {
        std::string err = "HTTP/1.0 502 Bad Gateway\r\n\r\nEmpty response\r\n";
        send(clientFd, err.data(), err.size(), 0);
        return false;
    }

    // Проверяем на редирект (3xx)
    auto pos = headers.find("\r\n");
    if (pos != std::string::npos) {
        std::string startLine = headers.substr(0, pos);
        if (startLine.find(" 3") != std::string::npos) {
            // Ищем Location
            auto locPos = headers.find("\r\nLocation:");
            if (locPos == std::string::npos) {
                locPos = headers.find("\r\nlocation:");
            }
            if (locPos != std::string::npos) {
                locPos += 2;
                auto endPos = headers.find("\r\n", locPos);
                if (endPos != std::string::npos) {
                    std::string locLine = headers.substr(locPos, endPos - locPos);
                    auto colonPos = locLine.find(':');
                    if (colonPos != std::string::npos) {
                        location = Utils::trim(locLine.substr(colonPos+1));
                        // Отправим заголовки клиенту - он увидит 3xx и Location
                        send(clientFd, headers.data(), headers.size(), 0);
                        return true;
                    }
                }
            }
        }
    }

    // Не редирект, отправляем заголовки клиенту
    send(clientFd, headers.data(), headers.size(), 0);
    return true;
}

bool ConnectionHandler::streamResponse(int serverFd, int clientFd) {
    char buf[4096];
    ssize_t n;
    while ((n = recv(serverFd, buf, sizeof(buf), 0)) > 0) {
        ssize_t totalSent = 0;
        while (totalSent < n) {
            ssize_t s = send(clientFd, buf + totalSent, n - totalSent, 0);
            if (s <= 0) return false;
            totalSent += s;
        }
    }
    return true;
}