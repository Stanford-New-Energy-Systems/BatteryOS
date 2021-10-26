#ifndef NETWORK_BATTERY_HPP
#define NETWORK_BATTERY_HPP

#include "BatteryInterface.hpp"
#include "Connections.hpp"
#include "include/json.hpp"
#include <memory>
/**
 * Refering to a distant battery through TCP connection
 * Protocol: 
 * first 4 bytes indicate the data length n, in little endian
 * then the next bytes indicate the data.
 */
class NetworkBattery : public Battery {
    std::string remote_name;
    std::unique_ptr<Connection> pconnection;
public:
    NetworkBattery(const std::string &name, const std::string &remote_name, const std::string &address, int port) : 
        Battery(name), remote_name(remote_name), pconnection() {
        
        this->pconnection.reset(new TCPConnection(address, port));
        if (!this->pconnection->connect()) {
            error("TCP connection failed!");
            // fprintf(stderr, "NetworkBattery: TCP connection failed\n");
            // exit(1);
        }
        this->refresh();  // also updates the timestamp
    }

    std::vector<uint8_t> send_recv(const std::vector<uint8_t> &request) {
        ssize_t num_bytes = this->pconnection->write(request);
        if (num_bytes < 0) {
            warning("Failed to send the request!");
            // fprintf(stderr, "NetworkBattery::send_recv: Failed to send the request\n");
            return {};
        }
        if (static_cast<size_t>(num_bytes) != request.size()) {
            warning("requested to send ", request.size(), " bytes, but sent ", num_bytes, " bytes");
            // fprintf(stderr, "NetworkBattery::send_recv: requested to send %lu bytes, but sent %lu bytes\n", (unsigned long)request.size(), (unsigned long)num_bytes);
        }
        std::vector<uint8_t> data = this->pconnection->read(4);  // 4 bytes to indicate the length
        if (data.size() != 4) {
            warning("unknown protocol");
            // fprintf(stderr, "NetworkBattery::send_recv: unknown protocol\n");
        }
        uint32_t data_length = uint32_t(data[3]) + (uint32_t(data[2]) << 8) + (uint32_t(data[1]) << 16) + (uint32_t(data[0]) << 24);

        data = this->pconnection->read(data_length);
        if (data.size() != data_length) {
            warning("data length mismatch!");
            // fprintf(stderr, "NetworkBattery::send_recv: data length mismatch\n");
        }
        return data;
    }
protected: 
    BatteryStatus refresh() override {
        lockguard_t lkd(this->lock);
        // do we really need JSON??? 
        nlohmann::json request_json;
        request_json["request"] = "get_status"; // should call refresh?
        request_json["name"] = this->remote_name;
        std::string request_str = request_json.dump();
        std::vector<uint8_t> request_bytes(request_str.begin(), request_str.end());
        std::vector<uint8_t> data = this->send_recv(request_bytes);
        BatteryStatus_deserialize(&(this->status), data.data(), (uint32_t)data.size());
        this->timestamp = get_system_time();
        return this->status;
    }
public:
    BatteryStatus get_status() override {
        return this->status;
    }

    uint32_t set_current(int64_t target_current_mA, bool is_greater_than_target, timepoint_t when_to_set) override {
        return 0;
    }

};

#endif // ! NETWORK_BATTERY_HPP




















