#ifndef SPLITTED_BATTERY_HPP
#define SPLITTED_BATTERY_HPP
#include "BatteryInterface.hpp"
#include "Policy.hpp"

class SplittedBattery : public VirtualBattery {
    std::string policy_name;
    BOSDirectory *pdirectory;
    SplitterPolicy *policy;
public: 
    SplittedBattery(
        const std::string &name, 
        BOSDirectory &directory,
        const std::chrono::milliseconds &sample_period 
    ) : 
        VirtualBattery(name, sample_period), 
        policy_name(), 
        pdirectory(&directory), 
        policy(nullptr)
    {
        this->type = BatteryType::Splitted;
    }
protected: 
    BatteryStatus refresh() override {
        if (!(this->policy)) {
            WARNING() << "battery is not attached to policy";
            return this->status;
        }
        BatteryStatus status = this->policy->get_status_of(this->name);
        this->status = status;
        return this->status;
    }

    uint32_t set_current(int64_t target_current_mA, bool is_greater_than_target, void *other_data) override {
        ERROR() << "This function shouldn't be called!";
        return 0;
    }
public: 
    bool attach_to_policy(const std::string &policy_name) {
        if (!this->pdirectory->name_exists(policy_name)) {
            WARNING() << "policy " << policy_name << " does not exist";
            return false;
        }
        Battery *b = this->pdirectory->get_battery(policy_name);
        if (!b) {
            WARNING() << "battery " << policy_name << " does not exist";
            return false;
        }
        if (b->get_battery_type() != BatteryType::SplitterPolicy) {
            WARNING() << "battery " << policy_name << " is not a splitter policy";
            return false;
        }
        SplitterPolicy *sp = dynamic_cast<SplitterPolicy*>(b);
        if (!sp) {
            WARNING() << "battery " << policy_name << " is not a splitter policy";
            return false;
        }
        this->policy = sp;
        this->policy_name = policy_name;
        return true; 
    }
    uint32_t schedule_set_current(
        int64_t target_current_mA, 
        bool is_greater_than_target, 
        timepoint_t when_to_set, 
        timepoint_t until_when
    ) override {
        lockguard_t lkg(this->lock);
        if (!(this->policy)) {
            WARNING() << "battery is not attached to policy";
            return 0;
        }
        // forward this to the Policy
        return this->policy->schedule_set_current_of(this->name, target_current_mA, is_greater_than_target, when_to_set, until_when);
    }


};

#endif // ! SPLITTED_BATTERY_HPP 































