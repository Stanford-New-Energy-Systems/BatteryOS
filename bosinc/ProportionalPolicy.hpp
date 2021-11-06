#ifndef PROPORTIONAL_POLICY_HPP
#define PROPORTIONAL_POLICY_HPP

#include "Policy.hpp"

struct Scale {
    double state_of_charge;
    double max_capacity;
    double max_discharge_rate;
    double max_charge_rate;
    static bool within_01_range(double num) { return 0.0 <= num && num <= 1.0; }
    Scale(double soc, double max_cap, double max_discharge_rate, double max_charge_rate) {
        if (!(within_01_range(soc) && within_01_range(max_cap) && within_01_range(max_discharge_rate) && within_01_range(max_charge_rate))) {
            WARNING() << "Scale parameter not within range [0.0, 1.0]";
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
            WARNING() << "Scale parameter not within range [0.0, 1.0]";
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
            WARNING() << ("not enough resource to subtract!");
            return Scale(0.0);
        }
    }

    Scale operator+(const Scale &other) {
        if (within_01_range(this->state_of_charge + other.state_of_charge) && 
            within_01_range(this->max_capacity + other.max_capacity) && 
            within_01_range(this->max_discharge_rate + other.max_discharge_rate) && 
            within_01_range(this->max_charge_rate >= other.max_charge_rate)) 
        {
            return Scale(
                this->state_of_charge + other.state_of_charge, 
                this->max_capacity + other.max_capacity, 
                this->max_discharge_rate + other.max_discharge_rate, 
                this->max_charge_rate + other.max_charge_rate);
        } else {
            WARNING() << ("sum not within [0, 1] range!");
            return Scale(0.0);
        }
    }
};

class ProportionalPolicy : public SplitterPolicy {
protected:
    std::map<std::string, Scale> scale_map;
public: 
    ProportionalPolicy(
        const std::string &policy_name, 
        const std::string &src_name, 
        BOSDirectory &directory, 
        Battery *first_battery,
        std::chrono::milliseconds max_staleness=std::chrono::milliseconds(1000)
    ) : 
        SplitterPolicy(policy_name, src_name, directory, SplitterPolicyType::Proportional, max_staleness)
    {
        // note: the first battery should be created and inserted already 
        this->children_current_now.insert(std::make_pair(first_battery->get_name(), 0));
        this->scale_map.insert(std::make_pair(first_battery->get_name(), Scale(1.0)));
    }

    /**
     * A refresh will update the status of all its children!  
     */
    BatteryStatus refresh() override {
        // lock should be already acquired!!! 
        BatteryStatus source_status = this->source->get_status();
        const std::list<Battery*> &children = this->pdirectory->get_children(this->name);
        int64_t total_actual_soc = source_status.state_of_charge_mAh;
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
            status.current_mA = children_current_now[child_name];
            status.state_of_charge_mAh = actual_soc;
            status.max_capacity_mAh = source_status.max_capacity_mAh * scale.max_capacity;
            status.max_charging_current_mA = source_status.max_charging_current_mA * scale.max_charge_rate;
            status.max_discharging_current_mA = source_status.max_discharging_current_mA * scale.max_discharge_rate;
            status.timestamp = get_system_time_c();
            children_status_now[child_name] = status;
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
            child->set_estimated_soc(source_status.state_of_charge_mAh * scale.state_of_charge);
        }
        return success;
    }
    
    
    BatteryStatus fork_from(
        const std::string &from_name, 
        const std::string &child_name, 
        BatteryStatus target_status
    ) override 
    {
        lockguard_t lkg(this->lock);
        if (!pdirectory->name_exists(from_name)) {
            WARNING() << "Battery " << from_name << " does not exist";
            return target_status;
        }
        if (!pdirectory->name_exists(child_name)) {
            WARNING() << "Battery " << child_name << " does not exist";
            return target_status;
        }
        if (!dynamic_cast<VirtualBattery*>(this->pdirectory->get_battery(child_name))) {
            WARNING() << "Battery " << child_name << " is not a virtual battery";
            return target_status;
        }

        Battery *from_battery = pdirectory->get_battery(from_name);
        BatteryStatus from_status = from_battery->get_status();

        if (from_status.current_mA != 0) {
            WARNING() << ("from battery is in use");
        }
        
        BatteryStatus actual_status;
        actual_status.voltage_mV = from_status.voltage_mV;
        actual_status.current_mA = 0;
        actual_status.state_of_charge_mAh = std::min(target_status.state_of_charge_mAh, from_status.state_of_charge_mAh);
        actual_status.max_capacity_mAh = std::min(target_status.max_capacity_mAh, from_status.max_capacity_mAh);
        actual_status.max_charging_current_mA = std::min(target_status.max_charging_current_mA, from_status.max_charging_current_mA);
        actual_status.max_discharging_current_mA = std::min(target_status.max_discharging_current_mA, from_status.max_discharging_current_mA);

        BatteryStatus source_status = this->source->get_status();

        // now compute the new scale 
        Scale scale(
            (double)actual_status.state_of_charge_mAh / source_status.state_of_charge_mAh,
            (double)actual_status.max_capacity_mAh / source_status.max_capacity_mAh,
            (double)actual_status.max_discharging_current_mA / source_status.max_discharging_current_mA,
            (double)actual_status.max_charging_current_mA / source_status.max_charging_current_mA
        );

        this->scale_map[child_name] = scale;
        this->children_current_now[child_name] = 0;
        this->scale_map[from_name] = this->scale_map[from_name] - scale;
        this->reset_estimated_soc_for_all();
        
        dynamic_cast<VirtualBattery*>(this->pdirectory->get_battery(child_name))->set_status(actual_status);
        return actual_status;
    }


    void merge_to(const std::string &name, const std::string &to_name) override {
        // Please think carefully on the locking, not sure how to implement this... 
        ERROR() << "Unimplemented"; 
        return;
    }


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






