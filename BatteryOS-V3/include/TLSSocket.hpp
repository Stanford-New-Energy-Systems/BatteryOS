#ifndef TLS_SOCKET_HPP
#define TLS_SOCKET_HPP

#include "NetService.hpp"
#include "Stream.hpp"
#include "Socket.hpp"
#include "util.hpp"

#include <functional>
#include <netinet/in.h>
#include <sys/socket.h>
#include <memory>
#include <utility>
#include <string>

#include <openssl/ssl.h>
#include <openssl/err.h>

class TLSSocket: public Socket {
    public:
        SSL* ssl;

        // TODO: remove fd from args
        TLSSocket(int fd);
        TLSSocket(int fd, std::function<void()> readHandler);
        ~TLSSocket();
        TLSSocket(const TLSSocket& other) = delete;
        TLSSocket(TLSSocket&& other) noexcept;
        TLSSocket& operator=(const TLSSocket& other) = delete;
        TLSSocket& operator=(TLSSocket&& other);

        static void InitializeServer(const std::string& ca_path, const std::string& cert_path, const std::string& key_path);
        static void InitializeClient(const std::string& ca_path, const std::string& cert_path, const std::string& key_path);

        // Stream
        size_t read(char* buffer, size_t len) override;
        size_t write(char* buffer, size_t len) override;
        size_t read_exact(char* buffer, size_t len) override;
        size_t write_exact(char* buffer, size_t len) override;

        // factory methods
        // TODO: maybe move this somewhere else
        // TODO: remove fd from args
        static std::unique_ptr<TLSSocket> connect(int fd, in_addr_t addr, int port);
        static TLSSocket* accept(int fd);

    private:
        static SSL_CTX* context;

};

#endif
