#ifndef AGGREGATE_BATTERY_HPP
#define AGGREGATE_BATTERY_HPP

#include "VirtualBattery.hpp"

/**
 * Aggregate Battery Class
 *
 * Aggregates physical or virtual batteries together 
 * to form a single virtual battery.
 *
 * @param parents:              list of parents that compose the virtual battery
 * @param eff_charge_c_rate:    the effective charge c rate of the battery
 * @param eff_discharge_c_rate: the effective discharge c rate of the battery
 *
 */

class AggregateBattery : public VirtualBattery {
    private:
        double eff_charge_c_rate;
        double eff_discharge_c_rate; 
        std::vector<std::shared_ptr<Battery>> parents;

    /**
     * Constructors
     * - Constructor should include battery name and the names of all the 
     *   source batteries (optional: max staleness and refresh mode) 
     */

    public:
        virtual ~AggregateBattery();
        AggregateBattery(const std::string &batteryName, 
                         std::vector<std::shared_ptr<Battery>> parentBatteries,
                         const std::chrono::milliseconds &maxStaleness = std::chrono::milliseconds(1000), 
                         const RefreshMode &refreshMode = RefreshMode::LAZY);
    
    /**
     * Private Helper Functions
     *
     * @func set_c_rate:     sets the effective charge/discharge c rate of the battery
     * @func calc_c_rate:    calculates the c rate of the battery 
     * @func calcStatusVals: calculates the battery status values of the battery 
     */
                                                                          
    private:
        void set_c_rate();
        BatteryStatus calcStatusVals();
        double calc_c_rate(const double &current_mA, const double &capacity_mAh);
    
    /**
     * Overridden Protected Function
     *
     * @func refresh: refreshes battery status
     */
    
    protected:
        BatteryStatus refresh() override;

    /**
     * Overridden Public Function
     *
     * @func schedule_set_current: schedules a set_current event for the battery 
     */

    public:
        std::string getBatteryString() const override;
        bool schedule_set_current(double current_mA, timepoint_t startTime, timepoint_t endTime, std::string name, uint64_t sequenceNumber) override;
};

#endif 
