#ifndef ACCEPTOR_HPP
#define ACCEPTOR_HPP

#include "NetService.hpp"
#include "Socket.hpp"
#include "TLSSocket.hpp"

#include <functional>
#include <sys/socket.h>
#include <netinet/in.h>

template <typename T>
class Acceptor : public Pollable {
    private:
        int fd;
        std::function<void(Socket *)> connectHandler;

    public:
        Acceptor(in_addr_t addr, int port, int backlog, std::function<void(Socket *)> connectHandler);
        virtual ~Acceptor();
        // TODO: write these out fully
        Acceptor(const Acceptor& other) = delete;
        Acceptor& operator=(const Acceptor& other) = delete;

        // Pollable
        struct pollfd pollInfo();
        void pollHandler();
};

// Definitions included here to deal with template linker issues...

#include <cerrno>
#include <cstring>
#include <unistd.h>
#include "util.hpp"

template <typename T>
Acceptor<T>::Acceptor(in_addr_t addr, int port, int backlog, std::function<void(Socket *)> connectHandler) 
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

template <typename T>
Acceptor<T>::~Acceptor() {
    shutdown(this->fd, SHUT_RDWR);
    close(this->fd);
}

// Pollable
template <typename T>
struct pollfd Acceptor<T>::pollInfo() {
    struct pollfd fd = {0};
    fd.fd = this->fd;
    fd.events = POLLIN;
    fd.revents = 0;
    return fd;
}

template <>
void Acceptor<TLSSocket>::pollHandler() {
    Socket* socket = TLSSocket::accept(accept(this->fd, NULL, NULL));
    this->connectHandler(socket);
}

template <>
void Acceptor<Socket>::pollHandler() {
    Socket* socket = new Socket(accept(this->fd, NULL, NULL));
    this->connectHandler(socket);
}

#endif
