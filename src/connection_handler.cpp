#include "connection_handler.hpp"
#include "logger.hpp"
#include "utils.hpp"
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sstream>
#include <cstring>

std::string ConnectionHandler::processRequest(const HttpRequest &req) {
    Logger::info("ConnectionHandler: processing request: " + req.method + " " + req.path);

    auto it = req.headers.find("host");
    if (it == req.headers.end()) {
        Logger::error("ConnectionHandler: No Host header found");
        return "HTTP/1.0 400 Bad Request\r\n\r\nHost header is required.\r\n";
    }

    std::string hostHeader = it->second;
    std::string host;
    int port;
    std::string path;
    if (!parseHostAndPort(hostHeader, host, port, path)) {
        Logger::error("ConnectionHandler: Invalid Host header: " + hostHeader);
        return "HTTP/1.0 400 Bad Request\r\n\r\nInvalid Host header.\r\n";
    }

    int serverFd = connectToServer(host, port);
    if (serverFd < 0) {
        Logger::error("ConnectionHandler: Could not connect to " + host + ":" + std::to_string(port));
        return "HTTP/1.0 502 Bad Gateway\r\n\r\nCould not connect to upstream server.\r\n";
    }

    if (!sendRequest(serverFd, req)) {
        Logger::error("ConnectionHandler: Failed to send request to server");
        close(serverFd);
        return "HTTP/1.0 502 Bad Gateway\r\n\r\nFailed to send request to server.\r\n";
    }

    std::string response = readResponse(serverFd);
    close(serverFd);
    Logger::info("ConnectionHandler: received response, size=" + std::to_string(response.size()));

    // Обработка редиректов:
    int redirectCount = 0;
    std::string location;
    while (isRedirect(response, location) && redirectCount < 5) {
        Logger::info("ConnectionHandler: redirect to " + location);
        std::string newHost;
        int newPort;
        std::string newPath;
        if (!parseHostAndPort(location, newHost, newPort, newPath)) {
            Logger::error("ConnectionHandler: invalid redirect location: " + location);
            return response;
        }
        serverFd = connectToServer(newHost, newPort);
        if (serverFd < 0) {
            Logger::error("ConnectionHandler: Could not connect to redirect location: " + newHost + ":" + std::to_string(newPort));
            return response;
        }
        HttpRequest newReq = req;
        newReq.path = newPath;
        // Host в запросе тоже нужно обновить, если редирект привел на другой домен
        newReq.headers["host"] = newHost + ((newPort != 80) ? (":" + std::to_string(newPort)) : "");

        if (!sendRequest(serverFd, newReq)) {
            Logger::error("ConnectionHandler: Failed to send request after redirect");
            close(serverFd);
            return response;
        }
        response = readResponse(serverFd);
        close(serverFd);
        redirectCount++;
    }

    return response;
}

bool ConnectionHandler::parseHostAndPort(const std::string &hostHeader, std::string &host, int &port, std::string &path) {
    port = 80;
    std::string h = Utils::trim(hostHeader);
    path = "/";

    std::string prefix_http = "http://";
    std::string prefix_https = "https://";
    bool isHttps = false;
    if (h.compare(0, prefix_http.size(), prefix_http) == 0) {
        h = h.substr(prefix_http.size());
    } else if (h.compare(0, prefix_https.size(), prefix_https) == 0) {
        h = h.substr(prefix_https.size());
        isHttps = true;
    }

    if (isHttps) {
        Logger::info("ConnectionHandler: https URL encountered, treating as http, port=80");
        port = 80;
    }

    // Ищем первый '/'
    auto slashPos = h.find('/');
    std::string hostPort = (slashPos == std::string::npos) ? h : h.substr(0, slashPos);
    if (slashPos != std::string::npos && slashPos+1 <= h.size()) {
        // всё, что после '/' - это путь
        path = h.substr(slashPos);
        if (path.empty()) path = "/";
    }

    auto pos = hostPort.find(':');
    if (pos == std::string::npos) {
        host = Utils::trim(hostPort);
    } else {
        host = Utils::trim(hostPort.substr(0, pos));
        std::string portStr = Utils::trim(hostPort.substr(pos+1));
        if (portStr.empty()) {
            return false;
        }
        try {
            port = std::stoi(portStr);
        } catch (...) {
            Logger::error("ConnectionHandler: invalid port in host header: " + portStr);
            return false;
        }
    }

    return !host.empty() && port > 0;
}

int ConnectionHandler::connectToServer(const std::string &host, int port) {
    Logger::info("ConnectionHandler: connecting to " + host + ":" + std::to_string(port));
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
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

std::string ConnectionHandler::readResponse(int serverFd) {
    Logger::info("ConnectionHandler: reading response from server");
    std::string response;
    char buf[4096];
    ssize_t n;
    while ((n = recv(serverFd, buf, sizeof(buf), 0)) > 0) {
        response.append(buf, n);
    }
    Logger::info("ConnectionHandler: response size = " + std::to_string(response.size()));
    return response;
}

bool ConnectionHandler::isRedirect(const std::string &response, std::string &location) {
    // Ищем код 3xx
    auto pos = response.find("\r\n");
    if (pos == std::string::npos) return false;
    std::string startLine = response.substr(0, pos);
    if (startLine.find(" 3") == std::string::npos) return false;

    auto locPos = response.find("\r\nLocation:");
    if (locPos == std::string::npos) {
        locPos = response.find("\r\nlocation:");
        if (locPos == std::string::npos) return false;
    }
    locPos += 2;
    auto endPos = response.find("\r\n", locPos);
    if (endPos == std::string::npos) return false;
    std::string locLine = response.substr(locPos, endPos - locPos);
    // locLine что-то вроде "Location: http://example.com"
    auto colonPos = locLine.find(':');
    if (colonPos == std::string::npos) return false;
    location = Utils::trim(locLine.substr(colonPos+1));
    Logger::info("ConnectionHandler: found redirect location: " + location);
    return !location.empty();
}
