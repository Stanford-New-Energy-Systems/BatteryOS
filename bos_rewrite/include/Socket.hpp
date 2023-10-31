#ifndef SOCKET_HPP
#define SOCKET_HPP

#include "NetService.hpp"
#include "Stream.hpp"

#include <functional>
#include <netinet/in.h>
#include <sys/socket.h>
#include <memory>
#include <utility>

// TODO: add a close handler to clean up connections properly...
// TODO: buffer the received input....
/**
 * A simple buffered socket class.
 * This base class usually shoudln't be used. Instead, use
 * a higher level abstraction like ProtobufPipe (TODO).
 */
class Socket : public Stream {
    protected:
        std::function<void()> readHandler;

    public:
        // TODO: make private
        int fd;

        // TODO: remove fd from args
        Socket(int fd);
        Socket(int fd, std::function<void()> readHandler);
        ~Socket();
        Socket(const Socket& other) = delete;
        Socket(Socket&& other) noexcept 
            : fd(std::exchange(other.fd, 0)), readHandler(std::exchange(other.readHandler, std::function<void()>())) {}
        Socket& operator=(const Socket& other) = delete;
        Socket& operator=(Socket&& other);

        void setReadHandler(std::function<void()> readHandler);

        // Stream
        size_t read(char* buffer, size_t len);
        size_t write(char* buffer, size_t len);
        size_t read_exact(char* buffer, size_t len);
        size_t write_exact(char* buffer, size_t len);

        // Pollable
        struct pollfd pollInfo();
        void pollHandler();

        // factory methods
        // TODO: maybe move this somewhere else
        // TODO: remove fd from args
        static std::unique_ptr<Socket> connect(int fd, in_addr_t addr, int port);
};

#endif
