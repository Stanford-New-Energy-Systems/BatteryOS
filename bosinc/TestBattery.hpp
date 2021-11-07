#ifndef TEST_BATTERY_HPP
#define TEST_BATTERY_HPP

#include "BatteryInterface.hpp"

/**
 * Virtual battery whose status are all 0, except for voltage
 */
class NullBattery : public PhysicalBattery {
public: 
    NullBattery(const std::string &name, int64_t voltage_mV, const std::chrono::milliseconds &max_staleness=std::chrono::milliseconds(1000)) : 
        PhysicalBattery(name, max_staleness) {
        this->status.voltage_mV = voltage_mV;
        this->status.current_mA = 0;
        this->status.capacity_mAh = 0;
        this->status.max_capacity_mAh = 0;
        this->status.max_charging_current_mA = 0;
        this->status.max_discharging_current_mA = 0;
    }
protected:
    BatteryStatus refresh() override {
        // no update on this->status
        this->status.timestamp = get_system_time_c();
        std::cout << "NullBattery: refresh" << std::endl;
        return this->status;
    }
    uint32_t set_current(int64_t target_current_mA, bool is_greater_than_target, void*) override {
        // if (target_current_mA != 0) {
        //     return 0;
        // }
        std::cout << "NullBattery: set current with target_current_mA = " << target_current_mA << ", and is_greater_than_target = " << is_greater_than_target << std::endl;
        return 1;
    }
public:
    std::string get_type_string() override {
        return "NullBattery";
    }
};

/**
 * A battery that returns the pre-specified status every time.
 */
class PseudoBattery : public PhysicalBattery {
public: 
    PseudoBattery(const std::string &name, const BatteryStatus &status) : PhysicalBattery(name, std::chrono::milliseconds(0)) {
        this->status = status;
    }
protected: 
    BatteryStatus refresh() override {
        this->status.timestamp = get_system_time_c();
        return this->status;
    }
    uint32_t set_current(int64_t target_current_mA, bool is_greater_than_target, void*) override {
        if (!(target_current_mA <= this->status.max_discharging_current_mA || 
            target_current_mA >= -(this->status.max_charging_current_mA))) {
            return 0;
        }
        this->status.current_mA = target_current_mA;
        return 1;
    }
public:
    BatteryStatus get_status() override {
        lockguard_t lkd(this->lock);
        return this->status;
    }

    void set_status(const BatteryStatus &new_status) {
        lockguard_t lkd(this->lock);
        this->status = new_status;
    }
};

#endif // ! TEST_BATTERY_HPP

