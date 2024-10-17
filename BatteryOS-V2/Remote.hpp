#pragma once 

#include "BatteryInterface.hpp"
#include "battery_msg.pb.h"
#include "admin_msg.pb.h"
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h> 
#define FAKE 1 
#if FAKE
#include <array>
#include <random>
#endif 
class RemoteBattery : public PhysicalBattery {
    std::string remote_name; 
    std::string address; 
    int port;
public: 
    RemoteBattery(
        const std::string &name, 
        const std::string &remote_name, 
        const std::string &addr, 
        const int port, 
        const std::chrono::milliseconds staleness
    ) : PhysicalBattery(name, staleness), 
        remote_name(remote_name), 
        address(addr), 
        port(port) 
    {}

    int connect(struct sockaddr_in &server_address) {
        int af_type = AF_INET;
        int socket_fd = socket(af_type, SOCK_STREAM, 0); 
        if (socket_fd < 0) {
            WARNING() << "socket not created, return value = " << socket_fd;
            return socket_fd;
        }
        memset(&server_address, 0, sizeof(server_address));
        server_address.sin_family = af_type;
        server_address.sin_port = htons(port);
        if (inet_pton(af_type, address.c_str(), &(server_address.sin_addr)) <= 0) {
            WARNING() << "inet_pton error";
            socket_fd = -1;
            return socket_fd;
        }
        if (::connect(socket_fd, (sockaddr*)&server_address, sizeof(server_address)) < 0) {
            WARNING() << "connect error";
            socket_fd = -2;
            return socket_fd;
        }
        return socket_fd; 
    }
    int disconnect(int socket_fd) {
        shutdown(socket_fd, SHUT_RDWR); 
        close(socket_fd); 
        return 0; 
    }

    BatteryStatus refresh() override {
        #if FAKE
        /*
        Battery 1
            Mean: 12.749ms
            Std: 8.163
        Battery 2
            Mean: 15.077ms
            Std: 8.037
        Battery 3
            Mean: 21.099ms
            Std:7.088
        Battery 4
            Mean: 34.369ms
            Std:8.219
        Battery 5
            Mean: 10.073ms
            Std: 4.241
        */
        using std::make_pair;
        std::array<std::pair<double, double>, 5> meanstd {
            make_pair(12.749, 8.163), 
            make_pair(15.077, 8.037), 
            make_pair(21.099, 7.088), 
            make_pair(34.369, 8.219), 
            make_pair(10.073, 4.241)
        };
        unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
        std::default_random_engine generator(seed); 
        
        // std::uniform_int_distribution<size_t> picker(0, 4); // both side inclusive 
        // size_t batidx = picker(generator); 
        size_t batidx = this->port % meanstd.size(); 
        std::normal_distribution<double> sampler(meanstd[batidx].first, meanstd[batidx].second); 
        double fakertt = sampler(generator); 
        std::this_thread::sleep_for(std::chrono::microseconds(static_cast<std::chrono::microseconds::rep>(fakertt/2.0 * 1000.0))); 
        #endif 
        sockaddr_in server_address; 
        int socket_fd = connect(server_address); 
        if (socket_fd < 0) {
            WARNING() << "connection failed to establish!"; 
            return {}; 
        }
        // LOG() << "connect success!"; 
        bool serialize_success = 0; 
        bosproto::MsgTag tag; 
        tag.set_tag(1);
        serialize_success = tag.SerializeToFileDescriptor(socket_fd); 
        if (!serialize_success) {
            WARNING() << "Tag failed to send!";
            disconnect(socket_fd); 
            return {}; 
        }
        // LOG() << "tag send success!"; 
        uint8_t read_buffer[4096]; 
        if (read(socket_fd, read_buffer, 4096) <= 0) {
            WARNING() << "server closed the connection already"; 
            disconnect(socket_fd);
            return {}; 
        }
        bosproto::BatteryMsg msg; 
        std::string *pmsgname = msg.mutable_name();
        pmsgname->assign(this->remote_name); 
        msg.mutable_get_status(); 
        serialize_success = msg.SerializeToFileDescriptor(socket_fd); 
        if (!serialize_success) {
            WARNING() << "Failed to serialize!";
            disconnect(socket_fd); 
            return {}; 
        }
        // LOG() << "msg send success!"; 
        bool parse_success = 0; 
        bosproto::BatteryResp resp; 
        ssize_t bytes_read = read(socket_fd, read_buffer, 4096); 
        if (bytes_read <= 0) {
            WARNING() << "The server closed the connection when reading battery status response!";
            disconnect(socket_fd); 
            return {};
        }
        parse_success = resp.ParseFromArray(read_buffer, bytes_read); 
        if (!parse_success) {
            WARNING() << "No response!"; 
            disconnect(socket_fd); 
            return {}; 
        }
        // LOG() << "response recv success!"; 
        disconnect(socket_fd); 
        bosproto::BatteryResp::RetvalCase rvc = resp.retval_case(); 
        if (rvc != bosproto::BatteryResp::RetvalCase::kStatus) {
            if (rvc == bosproto::BatteryResp::RetvalCase::kFailreason) {
                WARNING() << "no status is returned! fail reason: " << resp.failreason(); 
            } else {
                WARNING() << "no status is returned! and there's no fail reason either!";
            }
            return {}; 
        }
        const bosproto::BatteryStatus &stat = resp.status(); 
        this->status = BatteryStatus {
            .voltage_mV = stat.voltage_mv(), 
            .current_mA = stat.current_ma(),
            .capacity_mAh = stat.remaining_charge_mah(), 
            .max_capacity_mAh = stat.full_charge_capacity_mah(), 
            .max_charging_current_mA = stat.max_charging_current_ma(),
            .max_discharging_current_mA = stat.max_discharging_current_ma(), 
            .timestamp = CTimestamp {
                .secs_since_epoch = stat.timestamp().secs_since_epoch(), 
                .msec = stat.timestamp().msec()
            }
        };
        #if FAKE
        std::this_thread::sleep_for(std::chrono::microseconds(static_cast<std::chrono::microseconds::rep>(fakertt/2.0 * 1000.0))); 
        #endif 
        return this->status; 
    }

    uint32_t set_current(int64_t target_current_mA, bool is_greater_than_target, void *other_data) override {
        // this does nothing! the scheduling is done via the remote 
        return 0;
    }

    uint32_t schedule_set_current(
        int64_t target_current_mA, 
        bool is_greater_than_target, 
        timepoint_t when_to_set, 
        timepoint_t until_when
    ) override {
        sockaddr_in server_address; 
        int socket_fd = connect(server_address); 
        if (socket_fd < 0) {
            WARNING() << "connection failed to establish!"; 
            return {}; 
        }
        bool serialize_success = 0; 
        bosproto::MsgTag tag; 
        tag.set_tag(1);
        serialize_success = tag.SerializeToFileDescriptor(socket_fd); 
        if (!serialize_success) {
            WARNING() << "Tag failed to send!";
            disconnect(socket_fd); 
            return {}; 
        }
        uint8_t read_buffer[4096]; 
        if (read(socket_fd, read_buffer, 4096) <= 0) {
            WARNING() << "server closed the connection already"; 
            disconnect(socket_fd);
            return {}; 
        }
        CTimestamp fcts = timepoint_to_c_time(when_to_set); 
        CTimestamp tcts = timepoint_to_c_time(until_when); 
        bosproto::BatteryMsg msg; 
        std::string *pmsgname = msg.mutable_name();
        pmsgname->assign(this->remote_name); 

        bosproto::ScheduleSetCurrent *psc = msg.mutable_schedule_set_current(); 
        psc->set_target_current_ma(target_current_mA); 
        bosproto::CTimeStamp *pft = psc->mutable_when_to_set(); 
        pft->set_secs_since_epoch(fcts.secs_since_epoch); 
        pft->set_msec(fcts.msec); 

        bosproto::CTimeStamp *ptt = psc->mutable_until_when(); 
        ptt->set_secs_since_epoch(tcts.secs_since_epoch); 
        ptt->set_msec(tcts.msec); 





        serialize_success = msg.SerializeToFileDescriptor(socket_fd); 
        if (!serialize_success) {
            WARNING() << "Failed to serialize!";
            disconnect(socket_fd); 
            return {}; 
        }
        bool parse_success = 0; 
        bosproto::BatteryResp resp; 
        ssize_t bytes_read = read(socket_fd, read_buffer, 4096); 
        if (bytes_read <= 0) {
            WARNING() << "The server closed the connection when reading set_current response!";
            disconnect(socket_fd); 
            return {};
        }
        parse_success = resp.ParseFromArray(read_buffer, bytes_read); 
        if (!parse_success) {
            WARNING() << "No response!"; 
            disconnect(socket_fd); 
            return {}; 
        }
        disconnect(socket_fd); 
        auto rvc = resp.retval_case(); 
        if (rvc != bosproto::BatteryResp::RetvalCase::kSetCurrentResponse) {
            if (rvc == bosproto::BatteryResp::RetvalCase::kFailreason) {
                WARNING() << "set current does not get response! fail reason: " << resp.failreason(); 
            } else {
                WARNING() << "set current does not get response! and there's no fail reason either!";
            }
            return {}; 
        }
        return resp.set_current_response(); 
    }
    
};






