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

    BatteryStatus refresh() override {
        return this->status;
    }

    uint32_t set_current(int64_t target_current_mA, bool is_greater_than_target) override {
        return 0;
    }



};

#endif // ! SPLITTED_BATTERY_HPP 































