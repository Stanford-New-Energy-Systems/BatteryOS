#include "SecureClientBattery.hpp"

#include <thread>
#include "Socket.hpp"
#include "Fifo.hpp"
#include "BatteryConnection.hpp"

extern "C" {
    void client_submit(
        uint32_t* data,
        uint32_t data_len,
        uint32_t current_energy,
        uint32_t max_v,
        uint32_t max_e,
        void* user,
        void (*write_callback)(char*, uint32_t, uint32_t, void*)
        );
}

/***********************
Constructpor/Destructor 
************************/

SecureClientBattery::SecureClientBattery(int bat_port, int agg_port, const std::string& batteryName) {
    char buffer[64];
    struct sockaddr_in servAddr;
    int s = socket(AF_INET, SOCK_STREAM, 0);
    int s2 = socket(AF_INET, SOCK_STREAM, 0);

    std::unique_ptr<Socket> socket = Socket::connect(s, INADDR_ANY, bat_port);
    this->connection = std::make_unique<BatteryConnection>(std::move(socket));

    std::unique_ptr<Socket> socket2 = Socket::connect(s2, INADDR_ANY, agg_port);
    this->agg_connection = std::make_unique<BatteryConnection>(std::move(socket2));

    bosproto::BatteryConnect command;
    command.set_batteryname(batteryName);

    int success = connection->write(command);
    if (!success) {
        ERROR() << "Failed to send connection request: " << command.DebugString() << std::endl;
        exit(1);
    }

    // TODO: we should really write a nice wrapper around the message passing...
    //       TCP streams are not it...
    bosproto::ConnectResponse response;
    connection->read(response);

    if (response.status_code() == bosproto::ConnectStatusCode::DoesNotExist) {
        ERROR() << "server does not have battery in directory!" << std::endl;
        exit(1);
    } else if (response.status_code() == bosproto::ConnectStatusCode::DoesNotExist) {
        ERROR() << "generic battery connection error!" << std::endl;
        exit(1);
    }

    this->schedule = new uint32_t[1440];
    for (int i = 0; i < 1440; i++) {
        this->schedule[i] = 0;
    }
}

/****************
Public Functions
*****************/

BatteryStatus SecureClientBattery::getStatus() {
    bosproto::BatteryCommand command;
    command.set_command(bosproto::Command::Get_Status);

    this->connection->write(command);

    bosproto::BatteryStatusResponse response;
    int success = this->connection->read(response);
    std::cout << response.DebugString() << std::endl;

    if (!success) {
        WARNING() << "could not parse response" << std::endl;
        throw std::runtime_error("could not parse response");
    } else if (!response.has_status()) {
        WARNING() << "response did not include BatteryStatus" << std::endl;
        throw std::runtime_error("could not parse response");
    }

    return BatteryStatus(response.status());
} 

bool SecureClientBattery::setBatteryStatus(const BatteryStatus& status) {
    std::cout << "set battery status" << std::endl;
    bosproto::BatteryCommand command;
    bosproto::SetStatusResponse response; 

    command.set_command(bosproto::Command::Set_Status);
    bosproto::BatteryStatus* s = command.mutable_status();
    status.toProto(*s);

    // TODO: error
    this->connection->write(command);

    int success = this->connection->read(response);
    if (!success) {
        WARNING() << "could not parse response" << std::endl;
        return false;
    }

    if (response.return_code() == -1)
        return false;

    return true;
}

bool SecureClientBattery::schedule_set_current(double current_mA, uint64_t startTime, uint64_t endTime) {
    time_t start = (time_t) startTime;
    time_t end = (time_t) endTime;

    struct tm startTm;
    struct tm endTm;
    startTm = *localtime(&start);
    endTm = *localtime(&end);

    int start_min = startTm.tm_hour * 60 + startTm.tm_min;
    int end_min = endTm.tm_hour * 60 + endTm.tm_min;

    for (int i = start_min; i < end_min; i++) {
        this->schedule[i] = (uint32_t)current_mA;
    }
    return false;
}

bool SecureClientBattery::schedule_set_current(double current_mA, timestamp_t startTime, timestamp_t endTime) {
    uint64_t start = std::chrono::duration_cast<std::chrono::seconds>(startTime.time_since_epoch()).count();
    uint64_t end   = std::chrono::duration_cast<std::chrono::seconds>(endTime.time_since_epoch()).count();

    return this->schedule_set_current(current_mA, start, end); 
}

bool SecureClientBattery::submit_schedule() {
    client_submit(
        this->schedule,
        1440,
        2000,
        2000,
        2000,
        this,
        [](char* buf, uint32_t len, uint32_t i, void* user) {
            SecureClientBattery* self = (SecureClientBattery*)user;
            self->bytes.clear();
            self->bytes.insert(self->bytes.end(), (std::byte*)buf, (std::byte*)buf+len);

            bosproto::BatteryCommand command;
            bosproto::SetStatusResponse response; 
            command.set_command(bosproto::Command::Set_Schedule);
            command.mutable_set_schedule()->set_my_schedule(&self->bytes[0], self->bytes.size());
            std::cout << "GOT: " << command.DebugString() << std::endl;

            if (i == 0) {
                self->connection->write(command);
            } else {
                self->agg_connection->write(command);
            }
        }
    );

    //int success = this->connection->read(response);
    //if (!success) {
    //    WARNING() << "could not parse response" << std::endl;
    //    return false;
    //}

    //if (response.return_code() == -1)
    //    return false;

    return true;
}
