#ifndef AGGREGATOR_BATTERY_HPP
#define AGGREGATOR_BATTERY_HPP

#include "BatteryInterface.hpp"
#include "BOSDirectory.hpp"
/**
 * A virtual battery representing the aggregation of multiple batteries 
 */
class AggregatorBattery : public VirtualBattery {
    BOSDirectory *pdirectory;
    int64_t voltage_mV;
    int64_t voltage_tolerance_mV;
public: 
    AggregatorBattery(
        const std::string &name, 
        int64_t voltage_mV, 
        int64_t voltage_tolerance_mV, 
        const std::vector<std::string> &src_names, 
        BOSDirectory &directory, 
        const std::chrono::milliseconds &max_staleness=std::chrono::milliseconds(1000)) : 
        VirtualBattery(name, max_staleness),
        pdirectory(&directory),
        voltage_mV(voltage_mV),
        voltage_tolerance_mV(voltage_tolerance_mV)
    {
        bool add_success;
        Battery *bat;
        int64_t v; 
        for (const std::string &src : src_names) {
            bat = directory.get_battery(src);
            if (!bat) {
                warning("Failed to add battery ", name, "as source!");
                continue;
            }
            v = bat->get_status().voltage_mV;
            if (!(voltage_mV - voltage_tolerance_mV <= v && v <= voltage_mV + voltage_tolerance_mV)) {
                warning("Battery ", name, " is out of voltage tolerance, not added as source!");
                continue;
            }
            add_success = directory.add_edge(src, name);
            if (!add_success) {
                warning("Failed to add battery ", name, " as source!");
                continue;
            }
        }
    }

protected:
    BatteryStatus refresh() override {
        return this->status;
    }
    uint32_t set_current(int64_t current_mA, bool is_greater_than) override {
        return 0;
    }
public: 
    std::string get_type_string() override {
        return "AggregatorBattery";
    }

    BatteryStatus get_status() override {
        return this->status;
    }

    uint32_t schedule_set_current(int64_t target_current_mA, bool is_greater_than_target, timepoint_t when_to_set, timepoint_t until_when) override {
        return 0;
    }


};

#endif // ! AGGREGATOR_BATTERY_HPP 

