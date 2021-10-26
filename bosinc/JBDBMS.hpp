#ifndef JBDBMS_HPP
#define JBDBMS_HPP
#include "BatteryInterface.hpp"
#include <algorithm>
#include <sstream>
#include <array>
#include <tuple>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>


#include "Connections.hpp"
#include "RD6006.hpp"

class JBDBMS : public PhysicalBattery {
public:
    struct State {
        uint16_t voltage_10mV;
        int16_t current_10mA;
        uint16_t remaining_charge_10mAh;
        uint16_t max_capacity_10mAh;
        uint16_t cycles;
        uint16_t manufacture;
        uint16_t balancing_bit_1;
        uint16_t balancing_bit_2;
        uint16_t protection_state;
        uint8_t version;
        uint8_t remaining_percentage;
        uint8_t MOSFET_status;
        uint8_t num_batteries;
        uint8_t num_temperature_sensors;
        // throw away the temperature information
    };
    static constexpr uint8_t device_info_str[] = {0xdd, 0xa5, 0x03, 0x00, 0xff, 0xfd, 0x77};
    static constexpr uint8_t battery_info_str[] = {0xdd, 0xa5, 0x04, 0x00, 0xff, 0xfc, 0x77};
    static constexpr uint8_t version_info_str[] = {0xdd, 0xa5, 0x05, 0x00, 0xff, 0xfb, 0x77};

    static constexpr uint8_t enter_factory_mode_register = 0x00;
    static constexpr uint8_t exit_factory_mode_register = 0x01;

    static constexpr uint8_t enter_factory_mode_command[] = {0x56, 0x78};
    static constexpr uint8_t exit_and_save_factory_mode_command[] = {0x28, 0x28};
    static constexpr uint8_t exit_without_save_factory_mode_command[] = {0x00, 0x00};

    static constexpr uint8_t starter_byte = 0xdd;
    static constexpr uint8_t read_indicator_byte = 0xa5;
    static constexpr uint8_t write_indicator_byte = 0x5a;
    static constexpr uint8_t ending_byte = 0x77;
    
private:
    std::unique_ptr<Connection> pconnection;
    RD6006PowerSupply rd6006;
    std::chrono::milliseconds max_staleness_ms;
    int64_t max_charging_current_mA;
    int64_t max_discharging_current_mA;
    State basic_state;

public:
    static uint16_t compute_checksum(const std::vector<uint8_t> &payload);

    static std::vector<uint8_t> read_register_command(uint8_t register_address);

    static std::vector<uint8_t> write_register_command(uint8_t register_address, const std::vector<uint8_t> &bytes_to_write);

    static bool verify_recv_checksum(const std::vector<uint8_t> &header, const std::vector<uint8_t> &data, uint16_t checksum, uint8_t ending_byte);

    std::vector<uint8_t> query_info(const std::vector<uint8_t> &command);

    /// voltages: 100% 80% 60% 40% 20% 0%, all voltages are in mV, max_capacity is in 10mAh
    bool calibrate(uint16_t max_capacity_10mAh, const std::vector<uint16_t> &voltages_mV);
    

public: 
    JBDBMS(
        const std::string &name, 
        const std::string &device_address, 
        const std::string &current_regulator_address,
        const std::chrono::milliseconds &max_staleness_ms=std::chrono::milliseconds(100)
    );
    
    State get_basic_info();
    // basic_info: 
        // uint16_t voltage;
        // uint16_t current;
        // uint16_t remaining_charge;
        // uint16_t max_capacity;
        // uint16_t cycles;
        // uint16_t manufacture;
        // uint16_t balancing_bit_1;
        // uint16_t balancing_bit_2;
        // uint16_t protection_state;
        // uint8_t version;
        // uint8_t remaining_percentage;
        // uint8_t MOSFET_status;
        // uint8_t num_batteries;
        // uint8_t num_temperature_sensors;
    // end of basic_info

protected: 
    BatteryStatus refresh() override;
    bool check_staleness_and_refresh();
public: 
    std::string get_type_string() override;
    BatteryStatus get_status() override;
    
    /** > 0 discharging, < 0 charging */
    uint32_t set_current(int64_t target_current_mA, bool is_greater_than_target, timepoint_t when_to_set, timepoint_t until_when) override;
    
    bool set_max_staleness(int64_t new_max_staleness_ms);
    std::chrono::milliseconds get_max_staleness();
    
    

};

// note: we are overloading the << operator... 
std::ostream & operator<<(std::ostream &out, const JBDBMS::State &s);


#endif // ! JBDBMS_HPP

























