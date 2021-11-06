#include "AggregatorBattery.hpp"

AggregatorBattery::AggregatorBattery(
    const std::string &name, 
    int64_t voltage_mV, 
    int64_t voltage_tolerance_mV, 
    const std::vector<std::string> &src_names, 
    BOSDirectory &directory, 
    const std::chrono::milliseconds &max_staleness
) : 
    VirtualBattery(name, max_staleness),
    pdirectory(&directory),
    voltage_mV(voltage_mV),
    voltage_tolerance_mV(voltage_tolerance_mV)
{
    this->type = BatteryType::Aggregate;
    // bool add_success;
    Battery *bat;
    int64_t v; 
    for (const std::string &src : src_names) {
        bat = directory.get_battery(src);
        if (!bat) {
            ERROR() << "Battery " << name << "not found!";
            continue;
        }
        v = bat->get_status().voltage_mV;
        if (!(voltage_mV - voltage_tolerance_mV <= v && v <= voltage_mV + voltage_tolerance_mV)) {
            ERROR() << "Battery " << name << " is out of voltage tolerance!";
            continue;
        }
        // please add the topology outside of this! 
        // add_success = directory.add_edge(src, name);
        // if (!add_success) {
        //     WARNING() << "Failed to add battery " << name << " as source!";
        //     continue;
        // }
    }
}

BatteryStatus AggregatorBattery::refresh() {
    int64_t max_cap_mAh = 0;
    int64_t current_cap_mAh = 0;
    int64_t current_mA = 0;
    double max_discharge_time = 2147483647;
    double max_charge_time = 2147483647;
    const std::list<Battery*> &parents = pdirectory->get_parents(this->name);
    BatteryStatus status;
    for (Battery *bat : parents) {
        status = bat->get_status();
        if (!(voltage_mV - voltage_tolerance_mV <= status.voltage_mV && status.voltage_mV <= voltage_mV + voltage_tolerance_mV)) {
            WARNING() << "Battery " << bat->get_name() << " is out of voltage tolerance!";
        }
        max_cap_mAh += status.max_capacity_mAh;
        current_cap_mAh += status.state_of_charge_mAh;
        current_mA += status.current_mA;
        max_discharge_time = std::min(max_discharge_time, (double)status.state_of_charge_mAh / status.max_discharging_current_mA);
        max_charge_time = std::min(max_charge_time, (double)(status.max_capacity_mAh - status.state_of_charge_mAh) / status.max_charging_current_mA);
    }
    this->status.max_discharging_current_mA = (int64_t)(current_cap_mAh / max_discharge_time);
    this->status.max_charging_current_mA = (int64_t)((max_cap_mAh - current_cap_mAh) / max_charge_time);
    this->status.voltage_mV = this->voltage_mV;
    this->status.current_mA = current_mA;
    this->status.state_of_charge_mAh = current_cap_mAh;
    this->status.max_capacity_mAh = max_cap_mAh;
    this->status.timestamp = get_system_time_c();
    return this->status;
}

uint32_t AggregatorBattery::set_current(int64_t current_mA, bool is_greater_than, void *other_data) {
    // nothing here, use schedule_set_current instead 
    return 0;
}

std::string AggregatorBattery::get_type_string() { return "AggregatorBattery"; }

// BatteryStatus AggregatorBattery::get_status() {
//     lockguard_t lkd(this->lock);  // we should lock this if we are doing background refresh, or if not, leave it commented out
//     return this->refresh();  // always refresh, or do something else? 
// }

uint32_t AggregatorBattery::schedule_set_current(int64_t target_current_mA, bool is_greater_than_target, timepoint_t when_to_set, timepoint_t until_when) {
    // quickly refresh once, before the lock; 
    this->get_status();

    // forward the requests to its sources, immediately! 
    this->lock.lock();

    // int64_t old_current_mA = this->status.current_mA;
    int64_t max_discharge_current_mA = this->status.max_discharging_current_mA;
    int64_t max_charge_current_mA = this->status.max_charging_current_mA;

    if (target_current_mA > max_discharge_current_mA || (-target_current_mA) > max_charge_current_mA) {
        WARNING() << 
            "target current is out of range, max_charging_current = " << max_charge_current_mA << "mA"
            ", and max_discharging_current = " << max_discharge_current_mA << "mA"
            ", but target current = " << target_current_mA << "mA";
        
        this->lock.unlock();  // don't forget to unlock
        return 0;
    }

    const std::list<Battery*> &parents= pdirectory->get_parents(this->name);

    BatteryStatus status;
    int64_t current;

    if (target_current_mA > 0) {
        // discharging 
        double remaining_time = (double)this->status.state_of_charge_mAh / (double)target_current_mA;
        for (Battery *src : parents) {
            status = src->get_status();
            current = (int64_t)(status.state_of_charge_mAh / remaining_time);
            uint32_t success = src->schedule_set_current(current, is_greater_than_target, when_to_set, until_when);
            if (success == 0) {
                WARNING() << "Failed to set current for src battery? Current = " << current;
            }
        }
    } else if (target_current_mA < 0) {
        // charging 
        int64_t charge = this->status.max_capacity_mAh - this->status.state_of_charge_mAh;
        double charging_time = (double)charge / (double)-target_current_mA;
        for (Battery *src : parents) {
            status = src->get_status();
            current = (int64_t)((status.max_capacity_mAh - status.state_of_charge_mAh) / charging_time);
            uint32_t success = src->schedule_set_current(-current, is_greater_than_target, when_to_set, until_when);
            if (success == 0) {
                WARNING() << "Failed to set current for src battery? Current = " << current;
            }
        }
    } else {
        // target_current_mA == 0
        for (Battery *src : parents) {
            uint32_t success = src->schedule_set_current(0, is_greater_than_target, when_to_set, until_when);
            if (success == 0) {
                WARNING() << "Failed to set current for src battery? Current = " << 0;
            }
        }
    }

    this->lock.unlock();

    // next step? 

    return 1;
}




