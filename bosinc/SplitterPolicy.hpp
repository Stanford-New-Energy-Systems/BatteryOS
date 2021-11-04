#ifndef SPLITTER_POLICY_HPP
#define SPLITTER_POLICY_HPP

#include "BatteryInterface.hpp"
#include "BOSDirectory.hpp"
enum class SplitterPolicyType : int {
    Proportional, 
    Tranche, 
    Reservation,
};

class SplitterPolicy : public VirtualBattery {
protected: 
    std::string src_name;
    BOSDirectory *pdirectory;
    Battery *source;
    SplitterPolicyType policy_type;
public: 
    SplitterPolicy(
        const std::string &policy_name, 
        const std::string &src_name, 
        BOSDirectory &directory,
        SplitterPolicyType policy_type
    ) : 
        VirtualBattery(policy_name), 
        src_name(src_name), 
        pdirectory(&directory), 
        policy_type(policy_type)
    {
        this->type = BatteryType::SplitterPolicy;
        source = directory.get_battery(src_name);
        if (!source) {
            WARNING() << ("source not found!");
        }
    }
    
    BatteryStatus refresh() override {
        // nothing
        ERROR() << "This function shouldn't be called";
        return this->status;
    }
    uint32_t set_current(int64_t current_mA, bool is_greater_than) override {
        // nothing
        ERROR() << "This function shouldn't be called";
        return 0;
    }
    
    /**
     * Get its source battery 
     * @return Battery* pointing to its source battery 
     */
    virtual Battery *get_source() {
        return this->source;
    }

    /**
     * Get its children
     * @return a list of Battery*
     */
    virtual std::list<Battery*> get_children() {
        lockguard_t lkg(this->lock);
        return this->pdirectory->get_children(this->name);
    }

    /**
     * Receive and compute the status of one of its children
     */
    virtual BatteryStatus get_status_of(const std::string &child_name) = 0;

    /**
     * Schedule a set_current event for one of its children
     */
    virtual uint32_t schedule_set_current_of(
        const std::string &child_name, 
        int64_t target_current_mA, 
        bool is_greater_than_target, 
        timepoint_t when_to_set, 
        timepoint_t until_when
    ) = 0;

    /**
     * fork battery child_name from child_name, 
     * and the status is target_status (might not be fulfilled)
     * @param from_name the name of the battery to fork from
     * @param to_name the child battery, notice that this must be created first 
     * @param target_status the target status of the child battery 
     * @return the actual status of the child battery 
     */
    virtual BatteryStatus fork_from(
        const std::string &from_name, 
        const std::string &child_name, 
        BatteryStatus target_status
    ) = 0;

    /**
     * Merge a battery to another battery
     * @param name the name of the battery to merge 
     * @param to_name the name of the battery to receive the portion 
     * @return the status of the battery to_name after the merge 
     */
    virtual void merge_to(
        const std::string &name, 
        const std::string &to_name
    ) = 0;


};

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
            WARNING() << ("Scale parameter not within range [0.0, 1.0]");
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
            WARNING() << ("Scale parameter not within range [0.0, 1.0]");
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
    std::map<std::string, int64_t> current_map;
    std::map<std::string, Scale> scale_map;
public: 
    ProportionalPolicy(
        const std::string &policy_name, 
        const std::string &src_name, 
        BOSDirectory &directory, 
        Battery *first_battery
    ) : 
        SplitterPolicy(policy_name, src_name, directory, SplitterPolicyType::Proportional)
    {
        // note: the first battery should be created and inserted already 
        this->current_map.insert(std::make_pair(first_battery->get_name(), 0));
        this->scale_map.insert(std::make_pair(first_battery->get_name(), Scale(1.0)));
    }

    BatteryStatus get_status_of(const std::string &child_name) override {
        lockguard_t lkg(this->lock);

        Battery *source = this->source;

        BatteryStatus source_status = source->get_status();

        Scale &scale = this->scale_map[child_name];

        const std::list<Battery*> &children = this->pdirectory->get_children(this->name);

        int64_t estimated_soc = this->pdirectory->get_battery(child_name)->get_estimated_soc();

        int64_t total_estimated_soc = 0;

        for (Battery *c : children) {
            total_estimated_soc += c->get_estimated_soc();
        }
        int64_t total_actual_soc = source_status.state_of_charge_mAh;
        int64_t actual_soc = (int64_t)((double)estimated_soc / (double)total_estimated_soc * (double)total_actual_soc);
        
        BatteryStatus status;
        status.voltage_mV = source_status.voltage_mV;
        status.current_mA = current_map[child_name];
        status.state_of_charge_mAh = actual_soc;
        status.max_capacity_mAh = source_status.max_capacity_mAh * scale.max_capacity;
        status.max_charging_current_mA = source_status.max_charging_current_mA * scale.max_charge_rate;
        status.max_discharging_current_mA = source_status.max_discharging_current_mA * scale.max_discharge_rate;
        status.timestamp = get_system_time_c();
        return status;
    }

    uint32_t schedule_set_current_of(
        const std::string &child_name, 
        int64_t target_current_mA, 
        bool is_greater_than_target, 
        timepoint_t when_to_set, 
        timepoint_t until_when
    ) override 
    {
        lockguard_t lkg(this->lock);
        Scale &scale = this->scale_map[child_name];
        Battery *source = this->source;
        BatteryStatus source_status = source->get_status();

        if (target_current_mA > source_status.max_discharging_current_mA * scale.max_discharge_rate || 
            (-target_current_mA) > source_status.max_charging_current_mA * scale.max_charge_rate) {
            WARNING() << ("target current too high, event not scheduled");
            return 0;
        }
        this->current_map[child_name] = target_current_mA;
        int64_t new_currents = 0;
        for (auto &p : this->current_map) {
            new_currents += p.second;
        }
        source->schedule_set_current(new_currents, is_greater_than_target, when_to_set, until_when);
        return 0;
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
        if (pdirectory->name_exists(child_name)) {
            WARNING() << "Battery " << child_name << "exists";
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
        this->current_map[child_name] = 0;
        this->scale_map[from_name] = this->scale_map[from_name] - scale;
        this->reset_estimated_soc_for_all();
        return actual_status;
    }


    void merge_to(const std::string &name, const std::string &to_name) override {
        // TODO 
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

#endif // ! SPLITTER_POLICY_HPP
