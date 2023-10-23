#include "ClientBattery.hpp"

/***********************
Constructpor/Destructor 
************************/

ClientBattery::~ClientBattery() {
    close(this->clientSocket);
};

using namespace std::chrono_literals;
ClientBattery::ClientBattery(int port, const std::string& batteryName) : Battery(batteryName, 1000ms, RefreshMode::LAZY) {
    this->clientSocket = this->setupClient(port, batteryName);
}

/*****************
Private Functions
******************/

int ClientBattery::setupClient(int port, const std::string& batteryName) {
    char buffer[64];
    struct sockaddr_in servAddr;
    int s = socket(AF_INET, SOCK_STREAM, 0);

    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(port);
    servAddr.sin_addr.s_addr = INADDR_ANY; 

    int status = connect(s, (struct sockaddr*)&servAddr, sizeof(servAddr));

    if (status == -1)
        ERROR() << "could not connect to server!" << std::endl;

    LOG() << batteryName.c_str() << std::endl;

    int bytesWritten = write(s, batteryName.c_str(), batteryName.length());
    if (bytesWritten == -1)
        ERROR() << "could not write batteryName to socket!" << std::endl;

    if (recv(s, buffer, sizeof(buffer), MSG_PEEK | MSG_DONTWAIT) == 0)
        ERROR() << "server does not have battery in directory!" << std::endl;

    return s;
}

/****************
Public Functions
*****************/

BatteryStatus ClientBattery::getStatus() {
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
    command.SerializeToFileDescriptor(this->clientSocket);
    
    while ((fds[0].revents & POLLIN) != POLLIN) {
        int success = poll(fds, 1, -1);
        if (success == -1)
            ERROR() << "poll failed!" << std::endl;
    } 

    while (recv(this->clientSocket, buffer, size, MSG_PEEK) == size)
        size *= 2;

    int bytes = recv(this->clientSocket, buffer, size, 0);
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

bool ClientBattery::setBatteryStatus(const BatteryStatus& status) {
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

    command.SerializeToFileDescriptor(this->clientSocket);

    while ((fds[0].revents & POLLIN) != POLLIN) {
        int success = poll(fds, 1, -1);
        if (success == -1)
            ERROR() << "poll failed!" << std::endl;
    } 

    while (recv(this->clientSocket, buffer, size, MSG_PEEK) == size)
        size *= 2;

    int bytes = recv(this->clientSocket, buffer, size, 0);
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

bool ClientBattery::schedule_set_current(double current_mA, uint64_t startTime, uint64_t endTime) {
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

    command.SerializeToFileDescriptor(this->clientSocket);

    while ((fds[0].revents & POLLIN) != POLLIN) {
        int success = poll(fds, 1, -1);
        if (success == -1)
            ERROR() << "poll failed!" << std::endl;
    } 

    while (recv(this->clientSocket, buffer, size, MSG_PEEK) == size)
        size *= 2;

    int bytes = recv(this->clientSocket, buffer, size, 0);
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

bool ClientBattery::schedule_set_current(double current_mA, timestamp_t startTime, timestamp_t endTime) {
    uint64_t start = startTime.time_since_epoch().count();
    uint64_t end   = endTime.time_since_epoch().count();

    return this->schedule_set_current(current_mA, start, end); 
}
