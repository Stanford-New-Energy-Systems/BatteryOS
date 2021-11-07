#ifndef PROPORTIONAL_POLICY_HPP
#define PROPORTIONAL_POLICY_HPP

#include "Policy.hpp"

class ProportionalPolicy : public SplitterPolicy {
protected:
    std::map<std::string, Scale> scale_map;
public: 
    ProportionalPolicy(
        const std::string &policy_name, 
        const std::string &src_name, 
        BOSDirectory &directory, 
        const std::vector<std::string> &child_names, 
        const std::vector<Scale> &child_scales, 
        std::chrono::milliseconds max_staleness=std::chrono::milliseconds(1000)
    ) : 
        SplitterPolicy(
            policy_name, 
            src_name, 
            directory, 
            child_names, 
            child_scales, 
            SplitterPolicyType::Proportional, 
            max_staleness) 
    {
    }

    /**
     * A refresh will update the status of all its children!  
     */
    BatteryStatus refresh() override {
        // lock should be already acquired!!! 
        BatteryStatus source_status = this->source->get_status();
        const std::list<Battery*> &children = this->pdirectory->get_children(this->name);
        int64_t total_actual_soc = source_status.capacity_mAh;
        int64_t total_estimated_soc = 0;
        for (Battery *c : children) {
            total_estimated_soc += c->get_estimated_soc();
        }

        for (Battery *c : children) {
            const std::string &child_name = c->get_name();
            Scale &scale = this->scale_map[child_name];
            
            int64_t estimated_soc = c->get_estimated_soc();
            int64_t actual_soc = (int64_t)((double)estimated_soc / (double)total_estimated_soc * (double)total_actual_soc);
            BatteryStatus status;
            status.voltage_mV = source_status.voltage_mV;
            status.current_mA = children_current_now[this->name_lookup[child_name]];
            status.capacity_mAh = actual_soc;
            status.max_capacity_mAh = source_status.max_capacity_mAh * scale.max_capacity;
            status.max_charging_current_mA = source_status.max_charging_current_mA * scale.max_charge_rate;
            status.max_discharging_current_mA = source_status.max_discharging_current_mA * scale.max_discharge_rate;
            status.timestamp = get_system_time_c();
            this->children_status_now[this->name_lookup[child_name]] = status;
        }
        return this->status;  // meaningless 
    }

    bool reset_estimated_soc_for_all() {
        lockguard_t lkg(this->lock);
        BatteryStatus source_status = source->get_status();
        std::list<Battery*> children = this->get_children();
        decltype(this->scale_map)::iterator iter;
        bool success = true;
        for (Battery *child : children) {
            iter = this->scale_map.find(child->get_name());
            if (iter == this->scale_map.end()) {
                WARNING() << "Splitted Battery " << child->get_name() << " not found!";
                success = false;
                continue;
            }
            Scale &scale = iter->second;
            child->set_estimated_soc(source_status.capacity_mAh * scale.capacity);
        }
        return success;
    }
    
    
    // BatteryStatus fork_from(
    //     const std::string &from_name, 
    //     const std::string &child_name, 
    //     BatteryStatus target_status
    // ) override 
    // {
    //     lockguard_t lkg(this->lock);
    //     if (!pdirectory->name_exists(from_name)) {
    //         WARNING() << "Battery " << from_name << " does not exist";
    //         return target_status;
    //     }
    //     if (!pdirectory->name_exists(child_name)) {
    //         WARNING() << "Battery " << child_name << " does not exist";
    //         return target_status;
    //     }
    //     if (!dynamic_cast<VirtualBattery*>(this->pdirectory->get_battery(child_name))) {
    //         WARNING() << "Battery " << child_name << " is not a virtual battery";
    //         return target_status;
    //     }

    //     Battery *from_battery = pdirectory->get_battery(from_name);
    //     BatteryStatus from_status = from_battery->get_status();

    //     if (from_status.current_mA != 0) {
    //         WARNING() << "from battery is in use";
    //     }
        
    //     BatteryStatus actual_status;
    //     actual_status.voltage_mV = from_status.voltage_mV;
    //     actual_status.current_mA = 0;
    //     actual_status.capacity_mAh = std::min(target_status.capacity_mAh, from_status.capacity_mAh);
    //     actual_status.max_capacity_mAh = std::min(target_status.max_capacity_mAh, from_status.max_capacity_mAh);
    //     actual_status.max_charging_current_mA = std::min(target_status.max_charging_current_mA, from_status.max_charging_current_mA);
    //     actual_status.max_discharging_current_mA = std::min(target_status.max_discharging_current_mA, from_status.max_discharging_current_mA);

    //     BatteryStatus source_status = this->source->get_status();

    //     // now compute the new scale 
    //     Scale scale(
    //         (double)actual_status.capacity_mAh / source_status.capacity_mAh,
    //         (double)actual_status.max_capacity_mAh / source_status.max_capacity_mAh,
    //         (double)actual_status.max_discharging_current_mA / source_status.max_discharging_current_mA,
    //         (double)actual_status.max_charging_current_mA / source_status.max_charging_current_mA
    //     );

    //     this->scale_map[child_name] = scale;
    //     this->children_current_now[child_name] = 0;
    //     this->scale_map[from_name] = this->scale_map[from_name] - scale;
    //     this->reset_estimated_soc_for_all();
        
    //     dynamic_cast<VirtualBattery*>(this->pdirectory->get_battery(child_name))->set_status(actual_status);
    //     this->children_status_now[child_name] = actual_status;
    //     return actual_status;
    // }


    // void merge_to(const std::string &name, const std::string &to_name) override {
    //     // Please think carefully on the locking, not sure how to implement this... 
    //     ERROR() << "Unimplemented"; 
    //     return;
    // }


    // void merge_to(const std::string &name, const std::string &to_name) {
    //     lockguard_t lkg(this->lock);
    //     if (!pdirectory->name_exists(to_name)) {
    //         WARNING() << "Battery " << from_name << " does not exist";
    //         return;
    //     }
    //     if (!pdirectory->name_exists(name)) {
    //         WARNING() << "Battery " << child_name << "does not exist";
    //         return;
    //     }
    //     Battery *rbat = pdirectory->get_battery(name);
    //     Battery *to_bat = pdirectory->get_battery(to_name);
    //     Scale rm_scale = this->scale_map[rbat];
    //     int64_t rm_soc = rbat->get_estimated_soc();
    //     int64_t to_soc = to_bat->get_estimated_soc();

    //     to_soc += rm_soc;

    //     to_bat->set_estimated_soc(to_soc);
    // }

};




#endif // ! PROPORTIONAL_POLICY_HPP 






