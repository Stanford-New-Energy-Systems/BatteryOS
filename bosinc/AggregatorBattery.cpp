#include "AggregatorBattery.hpp"
#include <algorithm>
#include <execution>
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
    // to be initialized 
    this->status.voltage_mV = voltage_mV;
    this->status.current_mA = 0;
    this->status.capacity_mAh = 0;
    this->status.max_capacity_mAh = 0;
    /// let's not do anything in the experiment 
#define FAKE 1
#if not FAKE
    // bool add_success;
    Battery *bat;
    BatteryStatus pstatus;
    double max_discharge_time = 0;
    double max_charge_time = 0;
    for (const std::string &src : src_names) {
        bat = directory.get_battery(src);
        pstatus = bat->get_status();
        this->status.capacity_mAh += pstatus.capacity_mAh;
        this->status.max_capacity_mAh += pstatus.max_capacity_mAh;
        // this is the inv C-rate 
        max_discharge_time = std::max(max_discharge_time, (double)pstatus.capacity_mAh / pstatus.max_discharging_current_mA);
        max_charge_time = std::max(max_charge_time, (double)(pstatus.max_capacity_mAh - pstatus.capacity_mAh) / pstatus.max_charging_current_mA);
    }
    if (fabs(max_discharge_time) < 1e-6) {
        LOG() << "battery is empty: max_discharging_current is 0";
        this->status.max_discharging_current_mA = 1;
    } else {
        this->status.max_discharging_current_mA = (int64_t)(this->status.capacity_mAh / max_discharge_time);
    }
    if (fabs(max_charge_time) < 1e-6) {
        LOG() << "battery is full: max_charging_current is 0";
        this->status.max_charging_current_mA = 1;
    } else {
        this->status.max_charging_current_mA = (int64_t)((this->status.max_capacity_mAh - this->status.capacity_mAh) / max_charge_time);
    }
#else 
    this->status.max_discharging_current_mA = this->status.max_charging_current_mA = 1000;  // fake 
#endif 
    
}

BatteryStatus AggregatorBattery::refresh() {
    int64_t max_cap_mAh = 0;
    int64_t current_cap_mAh = 0;
    int64_t current_mA = 0;
    double max_discharge_time = 0;
    double max_charge_time = 0;
    const std::list<Battery*> &parents = pdirectory->get_parents(this->name);
    // parallelize? 
    std::mutex m;
    std::vector<std::thread> thread_pool(parents.size()); 
    int ti = 0; 
    for (Battery *bat : parents) {
        thread_pool[ti++] = std::thread(
            [
                &m, 
                &max_cap_mAh, 
                &current_cap_mAh, 
                &current_mA, 
                &max_discharge_time, 
                &max_charge_time,
                this
            ](Battery *bat) {
                BatteryStatus status = bat->get_status();
                if (!(this->voltage_mV - this->voltage_tolerance_mV <= status.voltage_mV && status.voltage_mV <= this->voltage_mV + this->voltage_tolerance_mV)) {
                    WARNING() << "Battery " << bat->get_name() << " is out of voltage tolerance!";
                }
                {
                    std::lock_guard<std::mutex> lg(m); 
                    max_cap_mAh += status.max_capacity_mAh;
                    current_cap_mAh += status.capacity_mAh;
                    current_mA += status.current_mA;
                    // this is the inv C-rate 
                    max_discharge_time = std::max(max_discharge_time, (double)status.capacity_mAh / status.max_discharging_current_mA);
                    max_charge_time = std::max(max_charge_time, (double)(status.max_capacity_mAh - status.capacity_mAh) / status.max_charging_current_mA);
                }
                return; 
            }, 
            bat
        );
    }
    for (auto &&t : thread_pool) {
        t.join(); 
    }
    if (fabs(max_charge_time) < 1e-6) {
        LOG() << "battery is full: max_charging_current is 0";
        this->status.max_charging_current_mA = 1;
    } else {
        this->status.max_charging_current_mA = (int64_t)((max_cap_mAh - current_cap_mAh) / max_charge_time);
    }
    if (fabs(max_discharge_time) < 1e-6) {
        LOG() << "battery is empty: max_discharging_current is 0";
        this->status.max_discharging_current_mA = 1;
    } else {
        this->status.max_discharging_current_mA = (int64_t)(current_cap_mAh / max_discharge_time);
    }
    this->status.voltage_mV = this->voltage_mV;
    this->status.current_mA = current_mA;
    this->status.capacity_mAh = current_cap_mAh;
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
    if (target_current_mA >= 0 && this->status.capacity_mAh == 0) {
        this->lock.unlock();
        return 0;
    } else if (target_current_mA < 0 && this->status.capacity_mAh >= this->status.max_capacity_mAh) {
        this->lock.unlock();
        return 0;
    }

    // int64_t old_current_mA = this->status.current_mA;
    int64_t max_discharge_current_mA = this->status.max_discharging_current_mA;
    int64_t max_charge_current_mA = this->status.max_charging_current_mA;

    if ((target_current_mA >= 0 && target_current_mA > max_discharge_current_mA) || 
        (target_current_mA < 0 && (-target_current_mA) > max_charge_current_mA)) {
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
        double remaining_time = (double)this->status.capacity_mAh / (double)target_current_mA;
        for (Battery *src : parents) {
            status = src->get_status();
            current = (int64_t)(status.capacity_mAh / remaining_time);
            uint32_t success = src->schedule_set_current(current, is_greater_than_target, when_to_set, until_when);
            if (success == 0) {
                WARNING() << "Failed to set current for src battery? Current = " << current;
            }
        }
    } else if (target_current_mA < 0) {
        // charging 
        int64_t charge = this->status.max_capacity_mAh - this->status.capacity_mAh;
        double charging_time = (double)charge / (double)(-target_current_mA);
        for (Battery *src : parents) {
            status = src->get_status();
            current = (int64_t)((status.max_capacity_mAh - status.capacity_mAh) / charging_time);
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




