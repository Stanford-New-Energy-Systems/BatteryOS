#ifndef TEST_BATTERY_HPP
#define TEST_BATTERY_HPP

#include "BatteryInterface.hpp"

/**
 * Virtual battery whose status are all 0, except for voltage
 */
class NullBattery : public Battery {
public: 
    NullBattery(const std::string &name, int64_t voltage_mV) : Battery(name) {
        this->status.voltage_mV = voltage_mV;
        this->status.current_mA = 0;
        this->status.state_of_charge_mAh = 0;
        this->status.max_capacity_mAh = 0;
        this->status.max_charging_current_mA = 0;
        this->status.max_discharging_current_mA = 0;
    }
protected:
    BatteryStatus refresh() override {
        // no update on this->status
        // required
        this->timestamp = get_system_time();
        return this->status;
    }
public:
    BatteryStatus get_status() override {
        // no need to add lock
        return this->refresh();
    }

    uint32_t set_current(int64_t target_current_mA, bool is_greater_than_target, timepoint_t when_to_set) override {
        if (target_current_mA != 0) {
            return 0;
        }
        return 1;
    }

    std::string get_type_string() override {
        return "NullBattery";
    }
};

/**
 * A battery that returns the pre-specified status every time.
 */
class PseudoBattery : public PhysicalBattery {
public: 
    PseudoBattery(const std::string &name, const BatteryStatus &status) : PhysicalBattery(name) {
        this->status = status;
        this->set_estimated_soc(this->status.state_of_charge_mAh);
    }
protected: 
    BatteryStatus refresh() override {
        this->timestamp = get_system_time();
        return this->status;
    }
public:
    BatteryStatus get_status() override {
        lockguard_t lkd(this->lock);
        int32_t old_soc = this->get_estimated_soc();
        this->update_estimated_soc(this->status.current_mA, this->status.current_mA);
        int32_t new_soc = this->get_estimated_soc();
        this->status.state_of_charge_mAh += (new_soc - old_soc);
        return this->status;
    }

    uint32_t set_current(int64_t target_current_mA, bool is_greater_than_target, timepoint_t when_to_set) override {
        lockguard_t lkd(this->lock);
        int64_t old_current = this->get_status().current_mA;
        if (!(target_current_mA <= this->status.max_discharging_current_mA || 
            target_current_mA >= -(this->status.max_charging_current_mA))) {
            return 0;
        }
        this->status.current_mA = target_current_mA;
        int64_t new_current = this->get_status().current_mA;
        this->update_estimated_soc(old_current, new_current);
        return 1;
    }

    void set_status(const BatteryStatus &new_status) {
        lockguard_t lkd(this->lock);
        this->update_estimated_soc(this->status.current_mA, new_status.current_mA);
        this->status = new_status;
    }
};

#endif // ! TEST_BATTERY_HPP

