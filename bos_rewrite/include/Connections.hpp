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
#include "wiringSerial.h"
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
    public:
        int serial_fd;
        bool connected;
        std::string device_path;
        static constexpr useconds_t WAIT_TIME = 50000;

    public:
        virtual ~UARTConnection(); 
        UARTConnection(UARTConnection &&other);
        UARTConnection(const std::string &device_path);
        UARTConnection(const UARTConnection &) = delete;

    public:
        void close() override;
        void flush() override;
        bool connect() override;
        bool is_connected() override;
        std::vector<uint8_t> read(int num_bytes) override;
        ssize_t write(const std::vector<uint8_t> &bytes) override;
};

#endif
