#ifndef ACCEPTOR_HPP
#define ACCEPTOR_HPP

#include "NetService.hpp"
#include "Socket.hpp"
#include "TLSSocket.hpp"

#include <functional>
#include <sys/socket.h>
#include <netinet/in.h>

class Acceptor : public Pollable {
    protected:
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

class TLSAcceptor : public Acceptor {
    public:
        void pollHandler() override;
        TLSAcceptor(in_addr_t addr, int port, int backlog, std::function<void(Socket *)> connectHandler) : Acceptor(addr, port, backlog, connectHandler) {}
};

#endif
