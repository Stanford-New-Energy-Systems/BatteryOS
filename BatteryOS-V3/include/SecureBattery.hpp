#ifndef SECURE_BATTERY_HPP
#define SECURE_BATTERY_HPP

#include <iostream>
#include "BatteryInterface.hpp"
#include "Socket.hpp"
#include "BatteryConnection.hpp"

extern "C" {
    void* new_collector(
        uint32_t id,
        uint32_t schedule_length,
        char* verify_key_ptr,
        uint32_t n_clients,
        uint32_t max_v,
        uint32_t max_e
    );

    void aggregate_coll(
        void* agg_ptr,
        const char* input_share,
        uint32_t input_share_len,
        uint32_t client_num
    );

    void add_prep(void* agg_ptr, const char* prep_share, uint32_t prep_share_len);
    void perform_round(void* agg_ptr, void* user,
        void (*write_callback)(char*, uint32_t, void*));
    void aggregate_finish_coll(void* agg_ptr, const char* prep_share, uint32_t prep_share_len);
    void collect(void* agg_ptr, const char* prep_share, uint32_t prep_share_len);
}


/**
 * Physical Battery Class
 * 
 * The physical battery represents an actual battery.
 * 
 * @func refresh:     refresh the status of the battery
 * @func set_current: set the current of the battery
 */
class SecureBattery: public Battery {
    // aggregator connection
    std::unique_ptr<BatteryConnection> connection;
    void* rust_coll;
    uint32_t client_num = 0;

    /**
     * Constructors
     * - Constructor should include battery name (optional: max staleness & refresh mode)
     */
    public:
        SecureBattery(const std::string &batteryName,
                      uint32_t num_clients,
                      char* verify_key,
                      int agg_port
                      );
    public:
        std::string getBatteryString() const override;
        bool set_schedule(const char* buffer, size_t len);
    
    protected:
        BatteryStatus refresh() override; 
        bool set_current(double current_mA) override;
};

#endif
