#include "SecureBattery.hpp"

using namespace std::chrono_literals;

SecureBattery::SecureBattery(const std::string &batteryName,
                            uint32_t num_clients,
                            char* verify_key,
                            int agg_port
                                 ) : Battery(batteryName,
                                             1000ms,
                                             RefreshMode::LAZY) 
{
    std::cout << "Creating secure battery..." << std::endl;
    this->type = BatteryType::Secure;

    struct sockaddr_in servAddr;
    int s = socket(AF_INET, SOCK_STREAM, 0);

    std::unique_ptr<Socket> socket = Socket::connect(s, INADDR_ANY, agg_port);
    this->connection = std::make_unique<BatteryConnection>(std::move(socket));

    this->rust_coll = new_collector(0, 1440, verify_key, 2, 2000, 2000);
}
    
std::string SecureBattery::getBatteryString() const {
    return "PhysicalBattery";
}
    
    
BatteryStatus SecureBattery::refresh() {
    PRINT() << "REFRESH!!!!" << std::endl;

    return this->status; 
}

bool SecureBattery::set_current(double current_mA) {
    PRINT() << "SET CURRENT: " << current_mA << "mA" << std::endl;
    this->status.current_mA = current_mA;
    return true; 
}

bool SecureBattery::set_schedule(const char* buffer, size_t len) {
    PRINT() << "SET SCHEDULE" << std::endl;
    bosproto::BatteryCommand command;

    PRINT() << "AGG COLL" << std::endl;
    aggregate_coll(
        this->rust_coll,
        buffer, 
        len,
        this->client_num++
    );

    PRINT() << "ADD PREP" << std::endl;
    // get prep message from aggregator
    int success = connection->read(command);
    bosproto::SetSchedule params = command.set_schedule();
    add_prep(this->rust_coll, params.my_schedule().c_str(), params.my_schedule().size());

    PRINT() << "PERFORM ROUND" << std::endl;
    // send back round message
    perform_round(this->rust_coll, this, [](char* bytes, uint32_t len, void* user) {
        SecureBattery* self = (SecureBattery*)user;
        bosproto::BatteryCommand command;
        command.set_command(bosproto::Command::Set_Schedule);
        command.mutable_set_schedule()->set_my_schedule(bytes, len);
        self->connection->write(command);
    });

    PRINT() << "FINISH COLL" << std::endl;
    // get out share from aggregator
    success = connection->read(command);
    params = command.set_schedule();
    aggregate_finish_coll(this->rust_coll, params.my_schedule().c_str(), params.my_schedule().size());

    PRINT() << "COLLECT" << std::endl;
    // get final agg from aggregator
    success = connection->read(command);
    params = command.set_schedule();
    collect(this->rust_coll, params.my_schedule().c_str(), params.my_schedule().size());

    // send prepare message
    if (this->client_num == 2) {
        //this->connection
    }

    return true;
}


