#ifndef SPLITTED_BATTERY_HPP
#define SPLITTED_BATTERY_HPP
#include "BatteryInterface.hpp"
#include "SplitterPolicy.hpp"

class SplittedBattery : public VirtualBattery {
    std::string policy_name;
    BOSDirectory *pdirectory;
    SplitterPolicy *policy;
public: 
    SplittedBattery(
        const std::string &name, 
        const std::chrono::milliseconds &sample_period, 
        const std::string &policy_name, 
        BOSDirectory &directory
    ) : 
        VirtualBattery(name, sample_period), 
        policy_name(policy_name)
    {
        this->type = BatteryType::SPLITTED;
        this->policy = dynamic_cast<SplitterPolicy*>(directory.get_battery(policy_name));
        this->pdirectory = &directory;
    }
protected: 
    BatteryStatus refresh() override {
        // do something 
        Battery *policy = pdirectory->get_battery(this->policy_name);
        if (policy->get_battery_type() != BatteryType::SPLIT_POLICY) {
            WARNING() << ("the policy is not a policy");
            return this->status;
        }
        // SplitterPolicy *spolicy = dynamic_cast<SplitterPolicy*>(policy);
        // BatteryStatus status = spolicy->get_status_of(spolicy->get_name());


        return this->status;
    }

    uint32_t set_current(int64_t target_current_mA, bool is_greater_than_target) override {
        // nothing here
        return 0;
    }
public: 
    BatteryStatus get_status() override {
        // always refresh?? 
        return this->status;
    }

    uint32_t schedule_set_current(int64_t target_current_mA, bool is_greater_than_target, timepoint_t when_to_set, timepoint_t until_when) override {
        // forward this to the Policy
        return 0;
    }




};

#endif // ! SPLITTED_BATTERY_HPP 































