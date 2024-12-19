#ifndef CONNECTION_HANDLER_HPP
#define CONNECTION_HANDLER_HPP

#include "http_parser.hpp"
#include <string>

class ConnectionHandler {
public:
    std::string processRequest(const HttpRequest &req);
private:
    bool parseHostAndPort(const std::string &hostHeader, std::string &host, int &port, std::string &path);
    int connectToServer(const std::string &host, int port);
    bool sendRequest(int serverFd, const HttpRequest &req);
    std::string readResponse(int serverFd);
    bool isRedirect(const std::string &response, std::string &location);
};

#endif // CONNECTION_HANDLER_HPP
