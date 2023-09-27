#ifndef ACCEPTOR_HPP
#define ACCEPTOR_HPP

#include "NetService.hpp"
#include "Socket.hpp"

#include <functional>
#include <sys/socket.h>
#include <netinet/in.h>

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

#endif
