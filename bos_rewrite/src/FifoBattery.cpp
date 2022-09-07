#include "FifoBattery.hpp"

FifoBattery::~FifoBattery() {};

FifoBattery::FifoBattery(const std::string& inputFilePath, const std::string& outputFilePath) {
    this->inputFilePath  = inputFilePath;
    this->outputFilePath = outputFilePath;
}

/***************
Public Funtions
****************/

BatteryStatus FifoBattery::getStatus() {
    BatteryStatus status;
    pollfd* fds = new pollfd[1];
    bosproto::BatteryCommand command;
    bosproto::BatteryStatusResponse response; 
    int inputFD  = open(this->inputFilePath.c_str(), O_WRONLY);
    int outputFD = open(this->outputFilePath.c_str(), O_RDONLY | O_NONBLOCK);

    if (inputFD == -1 || outputFD == -1)
        ERROR() << "could not open input FIFO or output FIFO: check file paths" << std::endl;

    fds[0].fd      = outputFD;
    fds[0].events  = POLLIN;
    fds[0].revents = 0;
    
    command.set_command(bosproto::Command::Get_Status);
    command.SerializeToFileDescriptor(inputFD);
    close(inputFD);
    
    while ((fds[0].revents & POLLIN) != POLLIN) {
        int success = poll(fds, 1, -1);
        if (success == -1)
            ERROR() << "poll failed!" << std::endl;
    } 

    int success = response.ParseFromFileDescriptor(outputFD); 

    delete[] fds;
    close(outputFD);

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

bool FifoBattery::setBatteryStatus(const BatteryStatus& status) {
    pollfd* fds = new pollfd[1];
    bosproto::BatteryCommand command;
    bosproto::SetStatusResponse response; 
    int inputFD  = open(this->inputFilePath.c_str(), O_WRONLY);
    int outputFD = open(this->outputFilePath.c_str(), O_RDONLY | O_NONBLOCK);

    if (inputFD == -1 || outputFD == -1)
        ERROR() << "could not open input FIFO or output FIFO: check file paths" << std::endl;

    fds[0].fd      = outputFD;
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

    command.SerializeToFileDescriptor(inputFD);
    close(inputFD);

    while ((fds[0].revents & POLLIN) != POLLIN) {
        int success = poll(fds, 1, -1);
        if (success == -1)
            ERROR() << "poll failed!" << std::endl;
    } 

    int success = response.ParseFromFileDescriptor(outputFD); 

    delete[] fds;
    close(outputFD);

    if (!success) {
        WARNING() << "could not parse response" << std::endl;
        return false;
    }

    if (response.return_code() == -1)
        return false;
    return true;
}

bool FifoBattery::schedule_set_current(double current_mA, uint64_t startTime, uint64_t endTime) {
    pollfd* fds = new pollfd[1];
    bosproto::BatteryCommand command;
    bosproto::ScheduleSetCurrentResponse response; 
    int inputFD  = open(this->inputFilePath.c_str(), O_WRONLY);
    int outputFD = open(this->outputFilePath.c_str(), O_RDONLY | O_NONBLOCK);
    
    if (inputFD == -1 || outputFD == -1)
        ERROR() << "could not open input FIFO or output FIFO: check file paths" << std::endl;

    fds[0].fd      = outputFD;
    fds[0].events  = POLLIN;
    fds[0].revents = 0;

    command.set_command(bosproto::Command::Schedule_Set_Current);
    bosproto::ScheduleSetCurrent* s = command.mutable_schedule_set_current();
    s->set_current_ma(current_mA);
    s->set_starttime(startTime);
    s->set_endtime(endTime);

    command.SerializeToFileDescriptor(inputFD);
    close(inputFD);

    while ((fds[0].revents & POLLIN) != POLLIN) {
        int success = poll(fds, 1, -1);
        if (success == -1)
            ERROR() << "poll failed!" << std::endl;
    } 

    int success = response.ParseFromFileDescriptor(outputFD); 

    delete[] fds;
    close(outputFD);

    if (!success) {
        WARNING() << "could not parse response" << std::endl;
        return false;
    }

    if (response.return_code() == -1)
        return false;
    return true;
}

bool FifoBattery::schedule_set_current(double current_mA, timestamp_t startTime, timestamp_t endTime) {
    uint64_t start = startTime.time_since_epoch().count();
    uint64_t end   = endTime.time_since_epoch().count();

    return this->schedule_set_current(current_mA, start, end); 
}
