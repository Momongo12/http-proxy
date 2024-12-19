#ifndef LISTENER_HPP
#define LISTENER_HPP

class Listener {
public:
    Listener() = default;
    ~Listener();
    bool startListening(int port);
    int getSocketFd() const;
    int acceptClient();

private:
    int sockfd = -1;
    bool setNonBlocking(int fd);
};

#endif // LISTENER_HPP
