#ifndef CONNECTION_HANDLER_HPP
#define CONNECTION_HANDLER_HPP

#include "http_parser.hpp"
#include <string>

class ConnectionHandler {
public:
    bool processRequest(const HttpRequest &req, int clientFd);

private:
    bool parseFinalUrl(const HttpRequest &req, std::string &host, int &port, std::string &path);
    bool parseRedirectUrl(const std::string &location, std::string &host, int &port, std::string &path);
    int connectToServer(const std::string &host, int port);
    bool sendRequest(int serverFd, const HttpRequest &req);
    bool readHeadersAndCheckRedirect(int serverFd, int clientFd, std::string &location);
    bool streamResponse(int serverFd, int clientFd);

    bool chunked = false;
    bool haveContentLength = false;
    size_t contentLength = 0;

    bool streamChunkedResponse(int serverFd, int clientFd);
    bool streamRawResponse(int serverFd, int clientFd);
    bool streamWithContentLength(int serverFd, int clientFd, size_t length);
};

#endif // CONNECTION_HANDLER_HPP
