#ifndef SPLITTER_POLICY_HPP
#define SPLITTER_POLICY_HPP

#include "BatteryInterface.hpp"
#include "BOSDirectory.hpp"
#include "BOSNode.h"

class SplitterPolicy : public BOSNode {
protected: 
    std::string src_name;
    BOSDirectory *pdirectory;
    Battery *source;
public: 
    SplitterPolicy(const std::string &src_name, BOSDirectory &directory) : src_name(src_name), pdirectory(&directory) {
        source = pdirectory->get_battery(src_name);
        if (!source) {
            warning("source not found!");
        }
    }
    Battery *get_source() {
        return this->source;
    }
};


class ProportionalPolicy : public SplitterPolicy {
public:
    struct Scale {
        double state_of_charge;
        double max_capacity;
        double max_discharge_rate;
        double max_charge_rate;
        static bool within_01_range(double num) {
            return 0.0 <= num && num <= 1.0;
        }
        Scale(double soc, double max_cap, double max_discharge_rate, double max_charge_rate) {
            if (!(within_01_range(soc) && within_01_range(max_cap) && within_01_range(max_discharge_rate) && within_01_range(max_charge_rate))) {
                warning("Scale parameter not within range [0.0, 1.0]");
                this->state_of_charge = this->max_capacity = this->max_discharge_rate = this->max_charge_rate = 0.0;
            } else {
                this->state_of_charge = soc;
                this->max_capacity = max_cap;
                this->max_discharge_rate = max_discharge_rate;
                this->max_charge_rate = max_charge_rate;
            }
        }
        Scale(double proportion=0.0) {
            if (!within_01_range(proportion)) {
                warning("Scale parameter not within range [0.0, 1.0]");
                this->state_of_charge = this->max_capacity = this->max_discharge_rate = this->max_charge_rate = 0.0;
            } else {
                this->state_of_charge = this->max_capacity = this->max_discharge_rate = this->max_charge_rate = proportion;
            }
        }
        Scale operator-(const Scale &other) {
            if (this->state_of_charge >= other.state_of_charge && 
                this->max_capacity >= other.max_capacity && 
                this->max_discharge_rate >= other.max_discharge_rate && 
                this->max_charge_rate >= other.max_charge_rate) 
            {
                return Scale(
                    this->state_of_charge - other.state_of_charge, 
                    this->max_capacity - other.max_capacity, 
                    this->max_discharge_rate - other.max_discharge_rate, 
                    this->max_charge_rate - other.max_charge_rate);
            } else {
                warning("not enough resource to subtract!");
                return Scale(0.0);
            }
        }
    };


protected:
    std::map<Battery*, int64_t> current_map;
    std::map<Battery*, Scale> scale_map;
    std::vector<Battery*> children;
public: 
    ProportionalPolicy(const std::string &src_name, BOSDirectory &directory, Battery *first_battery) : 
        SplitterPolicy(src_name, directory)
    {
        // note: the first battery should be created and inserted already 
        this->current_map.insert(std::make_pair(first_battery, 0));
        this->scale_map.insert(std::make_pair(first_battery, Scale(1.0)));
        this->children.push_back(first_battery);
    }

    std::vector<Battery*> get_children() {
        return this->children;
    }

    BatteryStatus get_status_of(Battery *child) {
        Battery *source = this->source;
        BatteryStatus source_status = source->get_status();
        Scale &scale = this->scale_map.find(child)->second;
        int64_t estimated_soc = child->get_estimated_soc();
        int64_t total_estimated_soc = 0;
        for (Battery *c : children) {
            total_estimated_soc += c->get_estimated_soc();
        }
        int64_t total_actual_soc = source_status.state_of_charge_mAh;
        int64_t actual_soc = (int64_t)((double)estimated_soc / (double)total_estimated_soc * (double)total_actual_soc);
        BatteryStatus status;
        status.voltage_mV = source_status.voltage_mV;
        status.current_mA = current_map[child];
        status.state_of_charge_mAh = actual_soc;
        status.max_capacity_mAh = source_status.max_capacity_mAh * scale.max_capacity;
        status.max_charging_current_mA = source_status.max_charging_current_mA * scale.max_charge_rate;
        status.max_discharging_current_mA = source_status.max_discharging_current_mA * scale.max_discharge_rate;
        return status;
    }

    uint32_t schedule_set_current_of(Battery *child, int64_t target_current_mA, bool is_greater_than_target, timepoint_t when_to_set, timepoint_t until_when) {
        // 
        return 0;
    }







};

#endif // ! SPLITTER_POLICY_HPP
