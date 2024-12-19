#ifndef CONNECTION_HANDLER_HPP
#define CONNECTION_HANDLER_HPP

#include "http_parser.hpp"
#include <string>

class ConnectionHandler {
public:
    // Теперь processRequest принимает clientFd, чтобы сразу слать ответ клиенту
    bool processRequest(const HttpRequest &req, int clientFd);

private:
    bool parseFinalUrl(const HttpRequest &req, std::string &host, int &port, std::string &path);
    bool parseRedirectUrl(const std::string &location, std::string &host, int &port, std::string &path);
    int connectToServer(const std::string &host, int port);
    bool sendRequest(int serverFd, const HttpRequest &req);
    bool streamResponse(int serverFd, int clientFd);
    bool isRedirect(const std::string &partialResponse, std::string &location);
    bool handleRedirects(const HttpRequest &originalReq, int clientFd, std::string &response, int redirectCount);
    bool readHeadersAndCheckRedirect(int serverFd, int clientFd, std::string &location);
};

#endif // CONNECTION_HANDLER_HPP
