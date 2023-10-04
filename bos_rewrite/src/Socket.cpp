
#include "Socket.hpp"

#include "util.hpp"

#include <unistd.h>
#include <cstring>

// TODO:
Socket::Socket(int fd) 
    : fd(fd) {}

Socket::Socket(int fd, std::function<void()> readHandler) 
    : fd(fd), readHandler(readHandler) {}

Socket::~Socket() {
    // TODO:
}

Socket& Socket::operator=(Socket&& other) {
    // TODO: free the resources of this first

    this->fd = other.fd;
    this->readHandler = other.readHandler;

    other.fd = 0;
    other.readHandler = std::function<void()>();
    
    return *this;
}

void Socket::setReadHandler(std::function<void()> readHandler) {
    this->readHandler = readHandler;
}

size_t Socket::read(char* buffer, size_t len) {
    return ::read(this->fd, buffer, len);
}

size_t Socket::write(char* buffer, size_t len) {
    return ::write(this->fd, buffer, len);
}

size_t Socket::read_exact(char* buffer, size_t bytes) {
    return util::read_exact(this->fd, buffer, bytes);
}

size_t Socket::write_exact(char* buffer, size_t bytes) {
    return util::write_exact(this->fd, buffer, bytes);
}

// Pollable
struct pollfd Socket::pollInfo() {
    return {
        this->fd,
        POLLIN,
        0
    };
}

void Socket::pollHandler() {
    this->readHandler();
}

std::unique_ptr<Socket> Socket::connect(int fd, in_addr_t addr, int port) {
    std::unique_ptr<Socket> socket = std::make_unique<Socket>(fd);

    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    saddr.sin_addr.s_addr = addr; 

    int status = ::connect(socket->fd, (struct sockaddr*)&saddr, sizeof(saddr));

    if (status == -1) {
        ERROR() << "could not connect to server! " << std::strerror(errno) << std::endl;
        exit(1);
    }

    return socket;
}
