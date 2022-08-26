#ifndef PARTITION_BATTERY_HPP
#define PARTITION_BATTERY_HPP

#include "VirtualBattery.hpp"
#include "PartitionManager.hpp"

/**
 * Partition Battery Class
 *
 * The partition battery class creates partitioned batteries  
 * of various charge/capacity values from a source battery.
 *
 * @param source:               source battery for partitioned batteries
 * @param thread:               background thread for charging/discharging batteries
 * @param runningThread:        indicates if background thread is running
 * @param requested_current_mA: requested current fir charging/discharging
 */

class PartitionBattery : public VirtualBattery {
    private:
        bool runningThread;
        std::thread thread;
        double requested_current_mA;
        std::weak_ptr<PartitionManager> source;

    /**
     * Constructor
     *
     * - Constructor requires battery name (optional: max staleness and refresh mode)
     */

    public:
        virtual ~PartitionBattery();
        PartitionBattery(const std::string &batteryName,    
                         const std::chrono::milliseconds &maxStaleness = std::chrono::milliseconds(1000), 
                         const RefreshMode &refreshMode = RefreshMode::LAZY);

    /**
     * Private Helper Functions
     *
     * @func runChargingThread: function that runs in background thread to charge/discharge battery
     */    

    private:
        void runChargingThread();

    /**
     * Protected Helper Functions
     *
     * @func refresh:     refresh the status of the battery 
     * @func set_current: set the current of the battery
     */

    protected:
        BatteryStatus refresh() override;
        bool set_current(double current_mA) override;

    /**
     * Public Helper Functions
     *
     * @func getSourceName:        gets the name of the source battery
     * @func setSourceBattery:     sets the source battery
     * @func schedule_set_current: schedules a set_current event
     */

    public:
        std::string getSourceName() const;
        std::string getBatteryString() const override;        
        void setSourceBattery(std::shared_ptr<PartitionManager> source);
        bool schedule_set_current(double current_mA, timepoint_t startTime, timepoint_t endTime, std::string name, uint64_t sequenceNumber) override;

};

#endif
