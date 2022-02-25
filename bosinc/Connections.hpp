#ifndef CONNECTIONS_HPP
#define CONNECTIONS_HPP
#include <vector>
#include <array>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <arpa/inet.h> 
#include "BatteryInterface.hpp"
#include "util.hpp"
#include "include/wiringSerial.h"
struct Connection {
    Connection() {}
    Connection(const Connection &) = delete;
    virtual ~Connection() {}
    virtual bool is_connected() = 0;
    virtual bool connect() = 0;
    virtual std::vector<uint8_t> read(int num_bytes) = 0;
    virtual ssize_t write(const std::vector<uint8_t> &bytes) = 0;
    virtual void close() = 0;
    virtual void flush() = 0;
};

struct UARTConnection : public Connection {
    static constexpr useconds_t WAIT_TIME = 50000;
    int serial_fd;
    std::string device_path;
    bool connected;
    UARTConnection(const std::string &device_path) : serial_fd(0), device_path(device_path), connected(false) {}
    UARTConnection(const UARTConnection &) = delete;
    UARTConnection(UARTConnection &&other) : serial_fd(other.serial_fd), device_path(std::move(other.device_path)), connected(other.connected) {
        other.serial_fd = 0;
        other.connected = false;
    }
    virtual ~UARTConnection() {
        if (serial_fd) {
            close();
        }
    }
    bool is_connected() override { return connected; }
    bool connect() override {
        serial_fd = serialOpen(device_path.c_str(), 9600);
        if (serial_fd < 0) {
            WARNING() << "failed to open serial port " << device_path;
            // fprintf(stderr, "UARTConnection::connect: failed to open serial port %s\n", device_path.c_str());
            serial_fd = 0;
            return false;
        }
        connected = true;
        usleep(WAIT_TIME);
        return true;
    }
    std::vector<uint8_t> read(int num_bytes) override {
        int num_bytes_available = serialDataAvail(serial_fd);
        std::vector<uint8_t> result;
        if (num_bytes_available < 0) {
            WARNING() << "number of bytes available is " << num_bytes_available;
            // fprintf(stderr, "UARTConnection::read: Warning: number of bytes available is %d\n", num_bytes_available);
            return result;
        }
        result.resize(num_bytes, 0);
        int num_bytes_read = 0;
        int total_bytes_read = 0;
        while (total_bytes_read < num_bytes) {
            num_bytes_read = ::read(
                serial_fd, 
                (result.data() + total_bytes_read), 
                (num_bytes - total_bytes_read));
            if (num_bytes_read < 0) {
                WARNING() << "num_bytes_read = " << num_bytes_read << ", total_bytes_read = " << total_bytes_read;
                // fprintf(stderr, "UARTConnection::read: Warning: num_bytes_read = %d, total_bytes_read = %d\n", num_bytes_read, total_bytes_read);
                return result;
            }
            total_bytes_read += num_bytes_read;
        }
        // int byte_read = 0;
        // for (int i = 0; i < num_bytes_at_least; ++i) {
        //     byte_read = serialGetchar(serial_fd);
        //     if (byte_read == -1) {
        //         fprintf(stderr, "UARTConnection::read: Warning: read timeout, requested nbytes = %d\n", num_bytes_at_least);
        //         break;
        //     }
        //     result.push_back((uint8_t)byte_read);
        // }
        // while (serialDataAvail(serial_fd) > 0) {
        //     byte_read = serialGetchar(serial_fd);
        //     if (byte_read == -1) {
        //         fprintf(stderr, "UARTConnection::read: Warning: read timeout, requested nbytes = %d\n", num_bytes_at_least);
        //         break;
        //     }
        //     result.push_back((uint8_t)byte_read);
        // }
        return result;
    }
    ssize_t write(const std::vector<uint8_t> &bytes) override {
        ssize_t nbytes = ::write(serial_fd, bytes.data(), bytes.size());
        if (nbytes < 0) {
            WARNING() << "write fails, nbytes = " << nbytes;
            // fprintf(stderr, "UARTConnection::write: Error, nbytes = %d\n", (int)nbytes);
        } else if (nbytes < (ssize_t)bytes.size()) {
            WARNING() << "not all bytes are written, written nbytes = " << nbytes;
            // fprintf(stderr, "UARTConnection::write: Warning, not all bytes are written, written nbytes = %d\n", (int)nbytes);
        }
        usleep(WAIT_TIME);
        return nbytes;
    }
    void close() override {
        serialClose(serial_fd);
        connected = false;
        serial_fd = 0;
    }
    void flush() override {
        serialFlush(serial_fd);
        usleep(WAIT_TIME);
    }
};


struct TCPConnection : public Connection {
    std::string address;
    int port;
    int af_type;
    int socket_fd;
    bool connected;
    struct sockaddr_in server_address;

    TCPConnection(const std::string &address, int port, int af_type=AF_INET) : 
        address(address), port(port), af_type(af_type), socket_fd(0), connected(false), server_address() {}
    TCPConnection(const TCPConnection &) = delete;
    TCPConnection(TCPConnection &&other) : 
        address(std::move(other.address)), 
        port(other.port), 
        af_type(other.af_type), 
        socket_fd(other.socket_fd), 
        connected(other.connected), 
        server_address(other.server_address) {
        other.socket_fd = 0;
        other.connected = false;
    }
    virtual ~TCPConnection() {
        if (socket_fd) {
            close();
        }
    }
    bool is_connected() override {
        return connected;
    }
    
    bool connect() override {
        socket_fd = socket(af_type, SOCK_STREAM, 0);
        if (socket_fd < 0) {
            WARNING() << "socket not created, return value = " << socket_fd;
            // fprintf(stderr, "TCPConnection::connect: Error: socket not created, return value = %d\n", socket_fd);
            socket_fd = 0;
            return false;
        }

        memset(&server_address, 0, sizeof(server_address));

        server_address.sin_family = af_type;

        server_address.sin_port = htons(port);

        if (inet_pton(af_type, address.c_str(), &(server_address.sin_addr)) <= 0) {
            WARNING() << ("inet_pton error");
            // fprintf(stderr, "TCPConnection::connect: Error: inet_pton error\n");
            socket_fd = 0;
            return false;
        } 

        if (::connect(socket_fd, (sockaddr*)&server_address, sizeof(server_address)) < 0) {
            WARNING() << ("connect error");
            // fprintf(stderr, "TCPConnection::connect: Error: connect error\n");
            socket_fd = 0;
            return false;
        }

        connected = true;
        return true;
    }
    
    std::vector<uint8_t> read(int num_bytes) override {
        std::vector<uint8_t> data;
        if (num_bytes <= 0) {
            return data;
        }
        data.resize(num_bytes, 0);
        int num_bytes_read = 0;
        int total_bytes_read = 0;
        while (total_bytes_read < num_bytes) {
            num_bytes_read = ::read(
                socket_fd, 
                (data.data() + total_bytes_read), 
                (num_bytes - total_bytes_read));
            if (num_bytes_read < 0) {
                WARNING() << "read error, num_bytes_read = " << num_bytes_read << ", total_bytes_read = " << total_bytes_read;
                // fprintf(stderr, "TCPConnection::read: Warning: num_bytes_read = %d, total_bytes_read = %d\n", num_bytes_read, total_bytes_read);
                return data;
            }
            total_bytes_read += num_bytes_read;
        }
        return data;
    }
    
    ssize_t write(const std::vector<uint8_t> &bytes) override {
        ssize_t nbytes = ::write(socket_fd, bytes.data(), bytes.size());
        if (nbytes < 0) {
            WARNING() << "write error, nbytes = " << nbytes;
            // fprintf(stderr, "TCPConnection::write: Error, nbytes = %d\n", (int)nbytes);
        } else if (nbytes < (ssize_t)bytes.size()) {
            WARNING() << "not all bytes are written, written nbytes = " << nbytes;
            // fprintf(stderr, "TCPConnection::write: Warning, not all bytes are written, written nbytes = %d\n", (int)nbytes);
        }
        return nbytes;
    }
    
    void close() override {
        ::shutdown(socket_fd, SHUT_RDWR);
        ::close(socket_fd);
        connected = false;
        socket_fd = 0;
    }
    /// read until the socket is empty!
    void flush() override {
        uint32_t dummy_byte = 0;
        while (::read(socket_fd, &dummy_byte, 2) > 0) {}
        return;
    }
};


struct FIFOConnection : public Connection {
    std::string path;
    bool connected;
    int fd;
    int permission;
    bool newfifo;
    FIFOConnection(const std::string &path, int permission, bool newfifo=false) : 
        path(path), 
        connected(false), 
        fd(0), 
        permission(permission),
        newfifo(newfifo) {}
    FIFOConnection(const FIFOConnection &) = delete;
    FIFOConnection(FIFOConnection &&other) : 
        path(std::move(other.path)), 
        connected(other.connected),
        fd(other.fd),
        permission(other.permission),
        newfifo(other.newfifo) {
        other.fd = 0;
        other.connected = false;
        other.permission = 0;
    }
    virtual ~FIFOConnection() {
        if (fd) {
            close();
        }
    }
    bool is_connected() override {
        return connected;
    }
    
    bool connect() override {
        if (newfifo) {
            if (mkfifo(path.c_str(), this->permission) < 0) {
                WARNING() << "mkfifo failed for fifo connection " << path;
                return false;
            }
        }
        // this->fd = open(path.c_str(), O_RDWR | O_NONBLOCK);
        this->fd = open(path.c_str(), O_RDWR);
        if (fd < 0) {
            WARNING() << "fifo failed to open " << path;
            return false;
        }
        connected = true;
        return true;
    }
    
    std::vector<uint8_t> read(int num_bytes) override {
        std::vector<uint8_t> data;
        if (num_bytes <= 0) {
            return data;
        }
        data.resize(num_bytes, 0);
        int num_bytes_read = 0;
        int total_bytes_read = 0;
        while (total_bytes_read < num_bytes) {
            num_bytes_read = ::read(
                this->fd, 
                (data.data() + total_bytes_read), 
                (num_bytes - total_bytes_read));
            if (num_bytes_read < 0) {
                WARNING() << "read error, num_bytes_read = " << num_bytes_read << ", total_bytes_read = " << total_bytes_read;
                return data;
            }
            total_bytes_read += num_bytes_read;
        }
        return data;
    }
    
    ssize_t write(const std::vector<uint8_t> &bytes) override {
        ssize_t nbytes = ::write(this->fd, bytes.data(), bytes.size());
        if (nbytes < 0) {
            WARNING() << "write error, nbytes = " << nbytes;
        } else if (nbytes < (ssize_t)bytes.size()) {
            WARNING() << "not all bytes are written, written nbytes = " << nbytes;
        }
        return nbytes;
    }
    
    void close() override {
        ::close(fd);
        if (newfifo) {
            ::unlink(path.c_str());
        }
        connected = false;
        fd = 0;
    }
    /// read until the fd is empty!
    void flush() override {
        uint32_t dummy_byte = 0;
        while (::read(fd, &dummy_byte, 2) > 0) {}
        return;
    }
};



#endif // ! CONNECTIONS_HPP




























