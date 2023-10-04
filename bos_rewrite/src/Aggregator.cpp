
#include "Aggregator.hpp"

Aggregator::Aggregator(int addr, int client_port, int agg_port, char* verify_key, NetService* servicer) : agg_port(agg_port), client_port(client_port), servicer(servicer) {
    this->rust_agg = new_aggregator(1, 1440, verify_key, 2, 2000, 2000);
    this->aggListener = std::make_shared<Acceptor>(addr, agg_port, 1, [this, verify_key](Socket* socket) {
        std::cout << "AGG CONNECTED" << std::endl;

        this->connection = std::make_shared<BatteryConnection>(std::unique_ptr<Stream>(socket));
        this->connection->messageReadyHandler = [this](BatteryConnection* connection) {
            std::cout << "AGG MESSAGE" << std::endl;
        };
        this->servicer->add(this->connection);
        std::cout << "connection is: " << (!this->connection) << std::endl;
    });
    servicer->add(this->aggListener);

    this->clientListener = std::make_shared<Acceptor>(addr, client_port, 1, [this](Socket* socket) {
        std::cout << "CLIENT CONNECTED" << std::endl;

        this->client_connection = std::make_shared<BatteryConnection>(std::unique_ptr<Stream>(socket));
        this->client_connection->messageReadyHandler = [this](BatteryConnection* connection) {
            bosproto::BatteryCommand command;
            PRINT() << "AGG: READ FROM AGGREGATOR" << std::endl;
            int success = connection->read(command);
            if (!success) {
                WARNING() << "could not parse BatteryCommand" << std::endl;
                return;
            } 

            if (command.command() != bosproto::Command::Set_Schedule) {
                WARNING() << "NON SCHEDULE COMMAND TO AGG" << std::endl;
                return;
            }

            bosproto::SetSchedule params = command.set_schedule();

            PRINT() << "AGG: AGGREGATE" << std::endl;
            aggregate(
                this->rust_agg,
                params.my_schedule().c_str(),
                params.my_schedule().size(),
                client_num++,
                this,
                [](char* bytes, uint32_t len, uint32_t client_num, void* user) {
                    Aggregator* self = (Aggregator*) user;
                    bosproto::BatteryCommand command;
                    command.set_command(bosproto::Command::Set_Schedule);
                    command.mutable_set_schedule()->set_my_schedule(bytes, len);
                    self->connection->write(command);
                }
            );


            success = connection->read(command);
            params = command.set_schedule();
            agg_finish_round(this->rust_agg, params.my_schedule().c_str(), params.my_schedule().size(), this,
                [](char* bytes, uint32_t len, void* user) {
                    Aggregator* self = (Aggregator*) user;
                    bosproto::BatteryCommand command;
                    command.set_command(bosproto::Command::Set_Schedule);
                    command.mutable_set_schedule()->set_my_schedule(bytes, len);
                    self->connection->write(command);
                }
            );

            send_agg_share(this->rust_agg, this,
                [](char* bytes, uint32_t len, void* user) {
                    Aggregator* self = (Aggregator*) user;
                    bosproto::BatteryCommand command;
                    command.set_command(bosproto::Command::Set_Schedule);
                    command.mutable_set_schedule()->set_my_schedule(bytes, len);
                    self->connection->write(command);
                }
            );
            
        };
        this->servicer->add(this->client_connection);
    });
    servicer->add(this->clientListener);
}
