#ifndef AGGREGATOR_BATTERY_HPP
#define AGGREGATOR_BATTERY_HPP

#include "BatteryInterface.hpp"
#include "BOSDirectory.hpp"
/**
 * A virtual battery representing the aggregation of multiple batteries 
 * NOTE: get_status always forward the requests immediately. 
 */
class AggregatorBattery : public VirtualBattery {
    BOSDirectory *pdirectory;
    int64_t voltage_mV;
    int64_t voltage_tolerance_mV;
public: 
    AggregatorBattery(
        const std::string &name, 
        int64_t voltage_mV, 
        int64_t voltage_tolerance_mV, 
        const std::vector<std::string> &src_names, 
        BOSDirectory &directory, 
        const std::chrono::milliseconds &max_staleness=std::chrono::milliseconds(0)
    );
protected:
    /**
     * Compute the aggregate status.
     * The algorithm (when discharging):
     * - max capacity (MC): sum of source max capacities
     * - state of charge (SOC): sum of source states of charge
     * - current: sum of source currents
     * - max discharging current (MDC):
     *   1. Compute SOC C-rate for each source battery, defined as (src.SOC / src.MDC)
     *   2. Take the minimum SOC C-rate among the sources and use this as the SOC C-rate of the aggregate battery.
     *   3. Multiply this aggregate SOC C-rate by the SOC to get the maximum discharging current.
     * - max charging current (MCC):
     *   1. Compute inverse SOC C-rate for each source battery, defined as ((src.MC - src.SOC) / src.MCC).
     *   2. Take the minimum inverse SOC C-rate among the sources and use this as the SOC C-rate of the aggregate battery.
     *   3. Multiply this aggregate inverse SOC C-rate by (MC - SOC) to get the maximum charging current.
     */
    BatteryStatus refresh() override;

    /** this function should not be called */
    uint32_t set_current(int64_t current_mA, bool is_greater_than) override;
public: 
    std::string get_type_string() override;

    // /** just forward to refresh */
    // BatteryStatus get_status() override;
    // NOTE: get_status() has default implementation 

    /**
     * Set the discharging/charging current of the aggregate battery.
     * Each source battery must be set to a discharging/charging current proportional to its SOC, 
     *   so that all source batteries will be depleted / fully charged at the same time.
     * The algorithm:
     *   1. Compute the time to discharge/charge (TTD/TTC) for the aggregate battery:
     *     TTD = agg.SOC / target_current; 
     *     TTC = (agg.MC - agg.SOC) / -target_current; 
     *   2. For each source battery src, set the current using the aggregator's TTD/TTC:
     *     src.set_current(src.SOC / TTD); 
     *     src.set_current(-(src.MC - src.SOC) / TTC); 
     */
    uint32_t schedule_set_current(int64_t target_current_mA, bool is_greater_than_target, timepoint_t when_to_set, timepoint_t until_when) override;


};

#endif // ! AGGREGATOR_BATTERY_HPP 


