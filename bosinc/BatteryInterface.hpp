#ifndef BATTERY_INTERFACE_HPP
#define BATTERY_INTERFACE_HPP
#include "BOSNode.h"
#include "BatteryStatus.h"

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <memory>
#include <utility>
#include <chrono>

#include "util.hpp"

/**
 * A Battery abstract class
 */
class Battery : public BOSNode {
protected: 
    /** name of the battery */
    const std::string name;

    /** the thread that refreshes the battery for a given sampling period */
    std::unique_ptr<std::thread> background_refresh_thread;
    /** the thread lock for the battery */
    std::recursive_mutex lock; 
    using lockguard_t = std::lock_guard<std::recursive_mutex>;

    /** the status of the battery */
    BatteryStatus status;

    // /** the timestamp of last refresh */
    // timepoint_t timestamp;

    /** the wait time in ms between two refreshes if automatic refresh is enabled */
    const int64_t sampling_period;

    /** estimated net charge of this battery. This must be initialized by BOS */
    int32_t estimated_soc;  

    /** is it doing background refresh? */
    bool should_background_refresh;
    /** tell the background refreshing thread that it should quit */
    bool should_cancel_background_refresh;

public: 
    Battery(const std::string &name, const int64_t sampling_period=1000);
    
    /// the lock is neither copyable nor moveable
    Battery(const Battery &) = delete;

    /// the lock is neither copyable nor moveable
    Battery(Battery &&) = delete;
    
    /// the lock is neither copyable nor moveable
    Battery &operator=(const Battery &) = delete;

    /// the lock is neither copyable nor moveable
    Battery &operator=(Battery &&) = delete;
    
    virtual ~Battery();

// Drivers: Implement the following functions
protected:
    /**
     * Refresh the battery status from the driver,
     * NOTE: Must update the battery status!!!
     * NOTE: Must update the timestamp!!! (in BatteryStatus)
     * NOTE: no need to check lock!!!
     * @return the new status of the battery
     */
    virtual BatteryStatus refresh() = 0;
public:
    /** 
     * Possibly check the staleness and refresh the status of the battery if the staleness is expired. 
     * NOTE: Must add lock!!!!!!
     * @return the status of the battery, possibly new or a bit stale
     */
    virtual BatteryStatus get_status() = 0;
    
    /**
     * Set the current to greater than / less than target current in a specific time. 
     * NOTE: Must add lock!!!!!!
     * @param target_current_mA the target current, target_current_mA > 0: discharging, target_current_mA < 0: charging
     * @param is_greater_than_target require whether the new current >= target_current_mA or <= target_current_mA
     * @param when_to_set a timestamp representing when to set the current to satisfy the request
     * @param until_when a timestamp representing when to stop setting the current
     * @return something? 
     */
    virtual uint32_t set_current(int64_t target_current_mA, bool is_greater_than_target, timepoint_t when_to_set, timepoint_t until_when) = 0;

    /**
     * Just return the type string of the battery driver
     * @return the string representing the type of the driver
     */
    virtual std::string get_type_string() {
        return "BatteryInterface";
    }
// Up here

    /**
     * Manually refresh the battery status from the driver,
     * @return the new status of the battery
     */
    BatteryStatus manual_refresh();

    /**
     * The name of the battery
     * @return the battery name
     */
    const std::string &get_name();

    /**
     * The battery refreshing function, this should be the main function of background refreshing thread!
     * @param bat the pointer to the Battery class
     */
    static void background_refresh_func(Battery *bat);

    /**
     * Start the background refreshing mechanism
     * This spawns a refreshing thread and creates a mutex for protection
     * @return whether the start is successful or not
     */
    bool start_background_refresh();

    /**
     * Stop the background refresh and clean up the thread and lock
     * @return whether the stop is successful or not
     */
    bool stop_background_refresh();

    /**
     * Approximate the total charge that has passed through the battery. 
     * This function is general enough to work for background refresh() updates, which will call 
     * this with `end_current == new_current`, as well as set_current() requests, which will generally
     * call this with `end_current != new_current`.
     * @param end_current the current that the battery has just been measured at (before modification).
     * @param new_current the current that the battery has just been set to (after modification).
     */
    void update_estimated_soc(int32_t end_current, int32_t new_current);

    /** 
     * Getter of estimated soc 
     * @return estimated soc
     */
    int32_t get_estimated_soc();

    /** 
     * Setter of estimated soc 
     * @param new_estimated_soc the new value 
     */
    void set_estimated_soc(int32_t new_estimated_soc);

    /**
     * Resets the estimated soc to the state of charge in status
     */
    void reset_estimated_soc();

};


class PhysicalBattery : public Battery {
public: 
    PhysicalBattery(const std::string &name, const int64_t sample_period=-1) : Battery(name, sample_period) {}
    std::string get_type_string() override {
        return "PhysicalBattery";
    }
};



#endif // ! BATTERY_INTERFACE_HPP





























