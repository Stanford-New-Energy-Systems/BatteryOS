#ifndef NETWORK_BATTERY_HPP
#define NETWORK_BATTERY_HPP

#include "BatteryInterface.hpp"
#include "Connections.hpp"
#include "RPC.hpp"
#include <memory>
#include <string.h>
/**
 * Refering to a distant battery through TCP connection
 * Protocol: send 4 bytes indicating the length of data first, in little-endian
 * then the data. 
 * 
 * The network battery only forwards requests to the remote battery. 
 */
class NetworkBattery : public Battery {
    std::string remote_name;
    std::unique_ptr<Connection> pconnection;
public:
    NetworkBattery(
        const std::string &name, 
        const std::string &remote_name, 
        // const std::string &address, 
        // int port,
        std::unique_ptr<Connection> pconn,
        std::chrono::milliseconds max_staleness=std::chrono::milliseconds(1000)
    ) : 
        Battery(name, max_staleness),
        remote_name(remote_name), 
        pconnection(std::move(pconn)) 
    {
        this->type = BatteryType::Physical;
        this->refresh();  // also updates the timestamp
    }

    std::vector<uint8_t> send_recv(const std::vector<uint8_t> &request) {
        ssize_t num_bytes = this->pconnection->write(request);
        if (num_bytes < 0) {
            WARNING() << ("Failed to send the request!");
            return {};
        }
        if (static_cast<size_t>(num_bytes) != request.size()) {
            WARNING() << "requested to send " << request.size() << " bytes, but sent " << num_bytes << " bytes";
        }
        std::vector<uint8_t> data = this->pconnection->read(4);  // 4 bytes to indicate the length
        if (data.size() != 4) {
            WARNING() << ("unknown protocol");
            return {};
        }
        uint32_t data_length = deserialize_int<uint32_t>(data.data());
        // uint32_t(data[0]) + (uint32_t(data[1]) << 8) + (uint32_t(data[2]) << 16) + (uint32_t(data[3]) << 24);

        data = this->pconnection->read(data_length);
        if (data.size() != data_length) {
            WARNING() << ("data length mismatch!");
        }
        return data;
    }
protected: 
    BatteryStatus refresh() override {
        // send the request 
        std::vector<uint8_t> request_bytes(sizeof(uint32_t) + sizeof(RPCRequestHeader) + remote_name.size() + 1);
        uint8_t *buffer_ptr = request_bytes.data() + sizeof(uint32_t);
        // NOTE: please do not call refresh, because the rmeote has its own refreshing pattern
        RPCRequestHeader header = new_RPC_header(RPCFunctionID::GET_STATUS, remote_name);
        size_t num_header_bytes = RPCRequestHeader_serialize(&header, buffer_ptr, request_bytes.size());
        buffer_ptr += num_header_bytes;
        for (size_t i = 0; i < remote_name.size(); ++i) {
            (*buffer_ptr) = uint8_t(remote_name[i]);
            buffer_ptr += 1;
        }
        (*buffer_ptr) = 0;
        uint32_t data_length = sizeof(RPCRequestHeader) + remote_name.size() + 1;
        size_t bytes_used = serialize_int<uint32_t>(data_length, request_bytes.data(), sizeof(uint32_t));
        if (bytes_used != sizeof(uint32_t)) {
            WARNING() << ("failed to serialize the data length");
            return this->status;
        }

        // now send the request
        std::vector<uint8_t> data = this->send_recv(request_bytes);
        BatteryStatus_deserialize(&(this->status), data.data(), (uint32_t)data.size());
        // timestamp is inside BatteryStatus
        return this->status;
    }
    uint32_t set_current(int64_t target_current_mA, bool is_greater_than_target, void *other_data) override {
        // this does nothing! the scheduling is done via the remote battery
        return 0;
    }
public:
    // BatteryStatus get_status() override {
    //     // lockguard_t lkd(this->lock);
    //     return this->refresh();
    // }

    uint32_t schedule_set_current(int64_t target_current_mA, bool is_greater_than_target, timepoint_t when_to_set, timepoint_t until_when) override {
        // lockguard_t lkd(this->lock);
        // send the request 
        std::vector<uint8_t> request_bytes(sizeof(uint32_t) + sizeof(RPCRequestHeader) + remote_name.size() + 1);
        uint8_t *buffer_ptr = request_bytes.data() + sizeof(uint32_t);
        RPCRequestHeader header = new_RPC_header(RPCFunctionID::SET_CURRENT, remote_name);
        header.current_mA = target_current_mA;
        header.is_greater_than = (int64_t)is_greater_than_target;
        header.when_to_set = timepoint_to_c_time(when_to_set);
        header.until_when = timepoint_to_c_time(until_when);
        size_t num_header_bytes = RPCRequestHeader_serialize(&header, buffer_ptr, request_bytes.size());
        buffer_ptr += num_header_bytes;
        for (size_t i = 0; i < remote_name.size(); ++i) {
            (*buffer_ptr) = uint8_t(remote_name[i]);
            buffer_ptr += 1;
        }
        (*buffer_ptr) = 0;
        uint32_t data_length = sizeof(RPCRequestHeader) + remote_name.size() + 1;
        size_t bytes_used = serialize_int<uint32_t>(data_length, request_bytes.data(), sizeof(uint32_t));
        if (bytes_used != sizeof(uint32_t)) {
            WARNING() << ("failed to serialize the data length");
            return 0;
        }
        // now send the request
        std::vector<uint8_t> data = this->send_recv(request_bytes);

        return 1;
    }

};

#endif // ! NETWORK_BATTERY_HPP




















