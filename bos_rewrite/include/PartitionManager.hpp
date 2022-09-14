#ifndef PARTITION_MANAGER_HPP
#define PARTITION_MANAGER_HPP

#include "scale.hpp"
#include "VirtualBattery.hpp"

/**
 * Partition Manager Class
 *
 * The partition manager manages the partitioned batteries
 * by passing the set_current values to the source battery
 * and also enforcing the partition policies.
 *
 * @param source:            source battery that is partitioned
 * @param children:          list of child partitioned batteries
 * @param policyType:        policy of partition (proportional, tranched, or reserved)
 * @param child_proportions: list of proportion types for children
 */

class PartitionManager : public VirtualBattery {
    private:
        PolicyType policyType;
        std::shared_ptr<Battery> source;
        std::vector<Scale> child_proportions;
        std::vector<std::weak_ptr<VirtualBattery>> children;
    
    /**
     * Constructor
     *
     * - Constructor requires partition manager name, list of proportions,
     *   the partition policy type, a pointer to the source battery, and a 
     *   list of pointer to the child batteries (the child batteries are
     *   created before the partition manager).
     */

    public:
        virtual ~PartitionManager();
        PartitionManager(const std::string &batteryName,
                         std::vector<Scale> proportions,
                         const PolicyType &partitionPolicyType,
                         std::shared_ptr<Battery> sourceBattery,
                         std::vector<std::weak_ptr<VirtualBattery>> childBatteries);
    
    /**
     * Private Helper Functions
     *
     * @func refreshTranched:     refreshes child batteries corresponding to tranched policy
     * @func refreshReserved:     refreshes child batteries corresponding to reserved policy 
     * @func refreshProportional: refreshes child batteries corresponding to proportional policy 
     */

    private:
        void refreshTranched(const BatteryStatus &pStatus);
        void refreshReserved(const BatteryStatus &pStatus);
        void refreshProportional(const BatteryStatus &pStatus);

    /**
     * Protected Helper Functions
     *
     * @func refresh:     refreshes status of battery
     * @func set_current: sets the current of battery
     */

    protected:
        BatteryStatus refresh() override;
        bool set_current(double current_mA) override;

    /**
     * Public Helper Functions
     *
     * @func initBatteryStatus:    sets the status of one of the child batteries 
     * @func schedule_set_current: schedules a set_current event 
     */

    public:
        std::string getBatteryString() const override;
        BatteryStatus initBatteryStatus(const std::string &childName);
        bool schedule_set_current(double current_mA, timepoint_t startTime, timepoint_t endTime, std::string name, uint64_t sequenceNumber) override;
 
};

#endif
