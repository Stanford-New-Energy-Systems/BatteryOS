#include "SecureClient.hpp"

/*********************
Constructor/Destructor
**********************/

SecureClient::~SecureClient() {
    SSL_free(this->ssl);
    SSL_CTX_free(this->context);
    close(this->clientSocket);
}

SecureClient::SecureClient(int port, const std::string& batteryName) {
    SSL_library_init();
    this->context = InitCTX();
    if (SSL_CTX_load_verify_locations(this->context, "../certs/ca_cert.pem", NULL) != 1)
        ERR_print_errors_fp(stderr);
    LoadCertificates(this->context, (char*) "../certs/client.pem", (char*) "../certs/client.key");
    SSL_CTX_set_verify(this->context, SSL_VERIFY_PEER, NULL);

    this->clientSocket = this->setupClient(port);

    this->ssl = SSL_new(this->context);
    SSL_set_fd(this->ssl, this->clientSocket);
    if (SSL_connect(this->ssl) == -1)
        ERR_print_errors_fp(stderr);

    LOG() << batteryName.c_str() << std::endl;

    int bytesWritten = SSL_write(this->ssl, batteryName.c_str(), batteryName.length());
    if (bytesWritten == -1)
        ERROR() << "could not write batteryName to socket!" << std::endl;

//    char buffer[64];
//    if (SSL_peek(this->ssl, buffer, sizeof(buffer)) == 0)
//        ERROR() << "server does not have battery in directory!" << std::endl;
    LOG() << "just finished SSL_write" << std::endl;
}

/****************
Private Functions
*****************/

SSL_CTX* SecureClient::InitCTX() {
    OpenSSL_add_all_algorithms();                   // Load crypto
    SSL_load_error_strings();                       // bring in and register error messages
    const SSL_METHOD* method = TLS_client_method(); // create new client-method instance
    SSL_CTX* ctx = SSL_CTX_new(method);             // create new context

    if (ctx == NULL) {
        ERR_print_errors_fp(stderr);
        abort();
    }
    return ctx;
}

void SecureClient::LoadCertificates(SSL_CTX* ctx, char* certFile, char* keyFile) {
    if (SSL_CTX_use_certificate_file(ctx, certFile, SSL_FILETYPE_PEM) <= 0)
        ERR_print_errors_fp(stderr);
    else if (SSL_CTX_use_PrivateKey_file(ctx, keyFile, SSL_FILETYPE_PEM) <= 0)
        ERR_print_errors_fp(stderr);
    else if (!SSL_CTX_check_private_key(ctx))
        ERR_print_errors_fp(stderr);
    else
        return;
    abort();
}

int SecureClient::setupClient(int port) {
    struct sockaddr_in servAddr;
    int s = socket(AF_INET, SOCK_STREAM, 0);

    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(port);
    servAddr.sin_addr.s_addr = INADDR_ANY; 

    int status = connect(s, (struct sockaddr*)&servAddr, sizeof(servAddr));

    if (status == -1)
        ERROR() << "could not connect to server!" << std::endl;

    return s;
}

/***************
Public Functions
****************/

BatteryStatus SecureClient::getStatus() {
    int size = 4096;
    char buffer[size];
    BatteryStatus status;
    pollfd* fds = new pollfd[1];
    bosproto::BatteryCommand command;
    bosproto::BatteryStatusResponse response; 

    fds[0].fd      = this->clientSocket;
    fds[0].events  = POLLIN;
    fds[0].revents = 0;
    
    command.set_command(bosproto::Command::Get_Status);

    LOG() << "right now I am here!" << std::endl;

    int commandSize = command.ByteSizeLong();
    command.SerializeToArray(buffer, commandSize);
    SSL_write(this->ssl, buffer, commandSize);

    LOG() << "I am now here!" << std::endl;
    
    while ((fds[0].revents & POLLIN) != POLLIN) {
        int success = poll(fds, 1, -1);
        if (success == -1)
            ERROR() << "poll failed!" << std::endl;
    } 

    //while (SSL_peek(this->ssl, buffer, size) == size)
    //    size *= 2;

    int bytes   = SSL_read(this->ssl, buffer, size);
    int success = response.ParseFromArray(buffer, bytes); 

    delete[] fds;

    if (!success) {
        WARNING() << "could not parse response" << std::endl;
        return status;
    } else if (!response.has_status()) {
        WARNING() << "response did not include BatteryStatus" << std::endl;
        return status;
    }

    bosproto::BatteryStatus s = response.status();

    status.voltage_mV = s.voltage_mv();
    status.current_mA = s.current_ma();
    status.capacity_mAh = s.capacity_mah();
    status.max_capacity_mAh = s.max_capacity_mah();
    status.max_charging_current_mA = s.max_charging_current_ma();
    status.max_discharging_current_mA = s.max_discharging_current_ma();
    status.time = s.timestamp();

    return status;
} 

bool SecureClient::setBatteryStatus(const BatteryStatus& status) {
    int size = 4096;
    char buffer[size];
    pollfd* fds = new pollfd[1];
    bosproto::BatteryCommand command;
    bosproto::SetStatusResponse response; 

    fds[0].fd      = this->clientSocket;
    fds[0].events  = POLLIN;
    fds[0].revents = 0;

    command.set_command(bosproto::Command::Set_Status);
    bosproto::BatteryStatus* s = command.mutable_status();
    s->set_voltage_mv(status.voltage_mV);
    s->set_current_ma(status.current_mA);
    s->set_capacity_mah(status.capacity_mAh);
    s->set_max_capacity_mah(status.max_capacity_mAh);
    s->set_max_charging_current_ma(status.max_charging_current_mA);
    s->set_max_discharging_current_ma(status.max_discharging_current_mA);
    s->set_timestamp(status.time);

    int commandSize = command.ByteSizeLong();
    command.SerializeToArray(buffer, commandSize);
    SSL_write(this->ssl, buffer, commandSize);

    while ((fds[0].revents & POLLIN) != POLLIN) {
        int success = poll(fds, 1, -1);
        if (success == -1)
            ERROR() << "poll failed!" << std::endl;
    } 

//    while (SSL_peek(this->ssl, buffer, size) == size)
//        size *= 2;
    LOG() << "about to call SSL_read in setBatteryStatus" << std::endl;

    int bytes   = SSL_read(this->ssl, buffer, size);
    int success = response.ParseFromArray(buffer, bytes); 

    delete[] fds;

    if (!success) {
        WARNING() << "could not parse response" << std::endl;
        return false;
    }

    if (response.return_code() == -1)
        return false;
    return true;
}

bool SecureClient::schedule_set_current(double current_mA, uint64_t startTime, uint64_t endTime) {
    int size = 4096;
    char buffer[size];
    pollfd* fds = new pollfd[1];
    bosproto::BatteryCommand command;
    bosproto::ScheduleSetCurrentResponse response; 
    
    fds[0].fd      = this->clientSocket;
    fds[0].events  = POLLIN;
    fds[0].revents = 0;

    command.set_command(bosproto::Command::Schedule_Set_Current);
    bosproto::ScheduleSetCurrent* s = command.mutable_schedule_set_current();
    s->set_current_ma(current_mA);
    s->set_starttime(startTime);
    s->set_endtime(endTime);

    int commandSize = command.ByteSizeLong();
    command.SerializeToArray(buffer, commandSize);
    SSL_write(this->ssl, buffer, commandSize);

    while ((fds[0].revents & POLLIN) != POLLIN) {
        int success = poll(fds, 1, -1);
        if (success == -1)
            ERROR() << "poll failed!" << std::endl;
    } 

    //while (SSL_peek(this->ssl, buffer, size) == size)
    //    size *= 2;

    int bytes = SSL_read(this->ssl, buffer, size);
    int success = response.ParseFromArray(buffer, bytes); 

    delete[] fds;

    if (!success) {
        WARNING() << "could not parse response" << std::endl;
        return false;
    }

    if (response.return_code() == -1)
        return false;
    return true;
}

bool SecureClient::schedule_set_current(double current_mA, timestamp_t startTime, timestamp_t endTime) {
    uint64_t start = startTime.time_since_epoch().count();
    uint64_t end   = endTime.time_since_epoch().count();

    return this->schedule_set_current(current_mA, start, end); 
}
