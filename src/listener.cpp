#include "listener.hpp"
#include "logger.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

Listener::~Listener() {
    if (sockfd >= 0) {
        close(sockfd);
    }
}

bool Listener::startListening(int port) {
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        Logger::error("Failed to create socket");
        return false;
    }

    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        Logger::error("Failed to set socket options");
        close(sockfd);
        sockfd = -1;
        return false;
    }

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        Logger::error("Failed to bind socket");
        close(sockfd);
        sockfd = -1;
        return false;
    }

    if (listen(sockfd, SOMAXCONN) < 0) {
        Logger::error("Failed to listen on socket");
        close(sockfd);
        sockfd = -1;
        return false;
    }

    if (!setNonBlocking(sockfd)) {
        Logger::error("Failed to set socket non-blocking");
        close(sockfd);
        sockfd = -1;
        return false;
    }

    Logger::info("Listening on port " + std::to_string(port));
    return true;
}

int Listener::getSocketFd() const {
    return sockfd;
}

int Listener::acceptClient() {
    int clientFd = accept(sockfd, nullptr, nullptr);
    return clientFd;
}

bool Listener::setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return false;
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) return false;
    return true;
}
