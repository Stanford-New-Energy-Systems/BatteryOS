#include "Acceptor.hpp"

#include <cerrno>
#include <cstring>
#include <unistd.h>
#include "util.hpp"

Acceptor::Acceptor(in_addr_t addr, int port, int backlog, std::function<void(Socket *)> connectHandler) 
    : connectHandler(connectHandler) {
    // TODO:
    this->fd = socket(AF_INET, SOCK_STREAM, 0);

    if (this->fd < 0) {
        ERROR() << "Failed to create acceptor socket: " << strerror(errno) << std::endl;   
        exit(1);
    }

    struct sockaddr_in saddr;
    saddr.sin_port = htons(port);
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = addr;

    int result = bind(this->fd, (struct sockaddr*)&saddr, sizeof(saddr));
    if (result < 0) {
        ERROR() << "Failed to bind socket: " << strerror(errno) << std::endl;   
        exit(1);
    }

    result = listen(this->fd, backlog);
    if (result < 0) {
        ERROR() << "Failed to listen on socket: " << strerror(errno) << std::endl;   
        exit(1);
    }
}

Acceptor::~Acceptor() {
    shutdown(this->fd, SHUT_RDWR);
    close(this->fd);
}

// Pollable
struct pollfd Acceptor::pollInfo() {
    struct pollfd fd = {0};
    fd.fd = this->fd;
    fd.events = POLLIN;
    fd.revents = 0;
    return fd;
}

void Acceptor::pollHandler() {
    Socket* socket = new Socket(accept(this->fd, NULL, NULL));
    this->connectHandler(socket);
}

void TLSAcceptor::pollHandler() {
    Socket* socket = TLSSocket::accept(accept(this->fd, NULL, NULL));
    this->connectHandler(socket);
}
