#ifndef VIRTUAL_BATTERY_HPP
#define VIRTUAL_BATTERY_HPP

#include <iostream>
#include "BatteryInterface.hpp"

/**
 * Virtual Battery Class
 *
 * The virtual battery is the base class for partitioned
 * and aggregate batteries.
 *
 * @func refresh:                  refresh the status of the battery
 * @func set_current:              set the current of the battery
 * @func setMaxChargingCurrent:    sets the charging current of the battery
 * @func setMaxDischargingCurrent: sets the discharging current of the battery
 */

class VirtualBattery: public Battery {
    /**
     * Constructors
     * - Constructor should include battery name (optional: max staleness & refresh mode)
     */
    public:
        virtual ~VirtualBattery();
        
        VirtualBattery(const std::string &batteryName,
                       const std::chrono::milliseconds &maxStaleness = std::chrono::milliseconds(1000), 
                       const RefreshMode &refreshMode = RefreshMode::LAZY);

    public:
        std::string getBatteryString() const override; 
        void setMaxChargingCurrent(double current_mA);
        void setMaxDischargingCurrent(double current_mA);

    protected:
        BatteryStatus refresh() override;
        bool set_current(double current_mA) override;
};

#endif 
