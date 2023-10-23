#ifndef SECURE_CLIENT_HPP
#define SECURE_CLIENT_HPP

#include <poll.h>
#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <resolv.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "util.hpp"
#include "BatteryStatus.hpp"
#include "protobuf/battery.pb.h"
#include "protobuf/battery_manager.pb.h"

class SecureClient {
    private:
        SSL* ssl;
        SSL_CTX* context;
        int clientSocket;
         
    public:
        ~SecureClient();
        SecureClient(int port, const std::string& batteryName);

    private:
        SSL_CTX* InitCTX();
        int setupClient(int port);
        void LoadCertificates(SSL_CTX* ctx, char* certFile, char* keyFile);

    public:
        BatteryStatus getStatus();
        bool setBatteryStatus(const BatteryStatus& status);
        bool schedule_set_current(double current_mA, uint64_t startTime, uint64_t endTime);
        bool schedule_set_current(double current_mA, timestamp_t startTime, timestamp_t endTime);
};

#endif 
