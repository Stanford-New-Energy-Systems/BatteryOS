#include "ClientBattery.hpp"

#include <thread>
#include "Socket.hpp"
#include "TLSSocket.hpp"
#include "Fifo.hpp"
#include "BatteryConnection.hpp"

/***********************
Constructpor/Destructor 
************************/

ClientBattery::~ClientBattery() {
    //close(this->clientSocket);
};

ClientBattery::ClientBattery(const std::string& directory, const std::string& batteryName) {
    std::unique_ptr<FifoPipe> pipe = std::make_unique<FifoPipe>(std::move(FifoPipe::connect(directory, batteryName)));
    this->connection = std::make_unique<BatteryConnection>(std::move(pipe));
}

ClientBattery::ClientBattery(int port, const std::string& batteryName) {
    TLSSocket::InitializeClient("../certs/ca_cert.pem", "../certs/client.pem", "../certs/client.key");
    char buffer[64];
    struct sockaddr_in servAddr;
    int s = socket(AF_INET, SOCK_STREAM, 0);

    std::unique_ptr<Socket> socket = TLSSocket::connect(s, INADDR_ANY, port);
    this->connection = std::make_unique<BatteryConnection>(std::move(socket));

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
}

/****************
Public Functions
*****************/

BatteryStatus ClientBattery::getStatus() {
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

bool ClientBattery::setBatteryStatus(const BatteryStatus& status) {
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

bool ClientBattery::schedule_set_current(double current_mA, uint64_t startTime, uint64_t endTime) {
    bosproto::BatteryCommand command;

    command.set_command(bosproto::Command::Schedule_Set_Current);
    bosproto::ScheduleSetCurrent* s = command.mutable_schedule_set_current();
    s->set_current_ma(current_mA);
    s->set_starttime(startTime);
    s->set_endtime(endTime);

    this->connection->write(command);

    bosproto::ScheduleSetCurrentResponse response; 
    int success =this->connection->read(response);
    if (!success) {
        WARNING() << "could not parse response" << std::endl;
        return false;
    }

    return true;

    //if (response.return_code() == -1)
    //    return false;

    //int size = 4096;
    //char buffer[size];
    //pollfd* fds = new pollfd[1];
    //bosproto::ScheduleSetCurrentResponse response; 
    //
    //fds[0].fd      = this->clientSocket;
    //fds[0].events  = POLLIN;
    //fds[0].revents = 0;

    //command.SerializeToFileDescriptor(this->clientSocket);

    //while ((fds[0].revents & POLLIN) != POLLIN) {
    //    int success = poll(fds, 1, -1);
    //    if (success == -1)
    //        ERROR() << "poll failed!" << std::endl;
    //} 

    //while (recv(this->clientSocket, buffer, size, MSG_PEEK) == size)
    //    size *= 2;

    //int bytes = recv(this->clientSocket, buffer, size, 0);
    //int success = response.ParseFromArray(buffer, bytes); 

    //delete[] fds;

    //return true;
}

bool ClientBattery::schedule_set_current(double current_mA, timestamp_t startTime, timestamp_t endTime) {
    uint64_t start = startTime.time_since_epoch().count();
    uint64_t end   = endTime.time_since_epoch().count();

    return this->schedule_set_current(current_mA, start, end); 
}
