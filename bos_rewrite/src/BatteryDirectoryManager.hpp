#ifndef BATTERY_DIRECTORY_MANAGER_HPP
#define BATTERY_DIRECTORY_MANAGER_HPP

#include "PhysicalBattery.hpp"
#include "AggregateBattery.hpp"
#include "PartitionBattery.hpp"
#include "BatteryDirectory.hpp"

template <typename T, typename U>
bool allequal(const T &t, const U &u) {
    return t == u;
}

template <typename T, typename U, typename... Others>
bool allequal(const T &t, const U &u, Others const &... args) {
    return (t == u) && allequal(u, args...);
}

/**
 * Battery Directory Manager
 *
 * Creates/Manages batteries within battery directory.
 *
 * @func directory:       directory that holds batteries
 * @func pManagerCounter: counter to create pManager name anytime a partition battery is created
 */

class BatteryDirectoryManager {
    private:
        uint64_t pManagerCounter;
        std::unique_ptr<BatteryDirectory> directory;

    public:
        BatteryDirectoryManager();
        ~BatteryDirectoryManager();

    /**
     * Private Helper Functions
     *
     * @func createPartitionManagerName: creates name of partition manager
     */
    
    private:
        std::string createPartitionManagerName(); 

    /**
     * Public Helper Functions
     *
     * @func getBattery:             gets a battery from the directory
     * @func removeBattery:          removes a battery from the directory
     * @func createPhysicalBattery:  creates a physical battery and inserts it into the directory
     * @func createAggregateBattery: creates an aggregate battery and inserts it into the directory
     * @func createParititonBattery: creates a parition battery and inserts it into the directory 
     */

    public:
        bool removeBattery(const std::string &name);
        std::shared_ptr<Battery> getBattery(const std::string &name) const;

        std::shared_ptr<Battery> createPhysicalBattery(const std::string &name,
                                                       const std::chrono::milliseconds &maxStaleness = std::chrono::milliseconds(1000), 
                                                       const RefreshMode &refreshMode = RefreshMode::LAZY);

        std::shared_ptr<Battery> createAggregateBattery(const std::string &name,
                                                        std::vector<std::string> parentNames,
                                                        const std::chrono::milliseconds &maxStaleness = std::chrono::milliseconds(1000), 
                                                        const RefreshMode &refreshMode = RefreshMode::LAZY);

        std::vector<std::shared_ptr<Battery>> createPartitionBattery(const std::string &sourceName,
                                                                     const PolicyType &policyType, 
                                                                     std::vector<std::string> names, 
                                                                     std::vector<Scale> child_proportions,
                                                                     std::vector<std::chrono::milliseconds> maxStalenesses,
                                                                     std::vector<RefreshMode> refreshModes);

        std::vector<std::shared_ptr<Battery>> createPartitionBattery(const std::string &sourceName,
                                                                     const PolicyType &policyType, 
                                                                     std::vector<std::string> names, 
                                                                     std::vector<Scale> child_proportions);
};

#endif
