#ifndef PHYSICAL_BATTERY_HPP
#define PHYSICAL_BATTERY_HPP

#include <iostream>
#include "BatteryInterface.hpp"

/**
 * Physical Battery Class
 * 
 * The physical battery represents an actual battery.
 * 
 * @func refresh:     refresh the status of the battery
 * @func set_current: set the current of the battery
 */
class PhysicalBattery: public Battery {
    /**
     * Constructors
     * - Constructor should include battery name (optional: max staleness & refresh mode)
     */
    public:
        virtual ~PhysicalBattery(); 
        
        PhysicalBattery(const std::string &batteryName,
                        const std::chrono::milliseconds &maxStaleness = std::chrono::milliseconds(1000), 
                        const RefreshMode &refreshMode = RefreshMode::LAZY); 
    public:
        std::string getBatteryString() const override;
    
    protected:
        BatteryStatus refresh() override; 
        bool set_current(double current_mA) override;
};

#endif
