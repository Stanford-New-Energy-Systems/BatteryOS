#ifndef BATTERY_INTERFACE_HPP
#define BATTERY_INTERFACE_HPP
#include "BOSNode.h"
#include "BatteryStatus.h"

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <tuple>
#include <memory>
#include <utility>
#include <chrono>

#include "util.hpp"

/**
 * A Battery abstract class
 */
class Battery : public BOSNode {
public:
    enum class RefreshMode {
        LAZY, 
        ACTIVE,
    };
    enum class Function : int {
        REFRESH = 0,
        SET_CURRENT_END = 1, 
        SET_CURRENT = 2,
        CANCEL_EVENT = 3,  
    };

    struct event_t {
        timepoint_t timepoint;
        uint64_t sequence_number;
        Function func;
        int64_t current_mA;
        bool is_greater_than;
        event_t(const timepoint_t &tp, uint64_t seq_num, Function fn, int64_t mA, bool is_greater_than) : 
            timepoint(tp),
            sequence_number(seq_num),
            func(fn),
            current_mA(mA),
            is_greater_than(is_greater_than) {}
        event_t() {}

    };
protected: 
    /** name of the battery */
    const std::string name;

    /** the status of the battery */
    BatteryStatus status;

    // /** the timestamp of last refresh */
    // timepoint_t timestamp;


    /** estimated net charge of this battery. This must be initialized by BOS */
    int32_t estimated_soc;  


    /** refresh mode, could be automatic background refreshing or refresh based on staleness */
    RefreshMode refresh_mode;

    /** 
     * the wait time in ms, 
     * the time between two refreshes if automatic refresh is enabled, 
     * or the max staleness tolerance if refresh mode is based on staleness 
     */
    std::chrono::milliseconds max_staleness;

////// Event scheduling fields! 
    /** the thread that refreshes the battery for a given sampling period, and handles set_current events */
    std::thread background_thread;

    using lock_t = std::mutex;
    /** the thread lock for the battery */
    lock_t lock; 

    /** the condition variable to implement the event scheduling */
    std::condition_variable cv;

    /** the type for the lockguard */
    using lockguard_t = std::lock_guard<lock_t>;

    /** tell the background thread that it should quit */
    bool should_quit;
    
    /** event_t: at what time, do what, if set current what current, if set current is it greater than or less than */
    // using event_t = std::tuple<timepoint_t, Function, int64_t, bool>;

    /** the queue holding the events */
    std::priority_queue<event_t, std::vector<event_t>, std::greater<event_t>> event_queue;

    uint64_t current_sequence_number;
public: 
    /**
     * Constructor
     * @param name the name of the battery
     * @param max_staleness_ms the maximum staleness in milliseconds, notice that in background refresh mode, 
     *   the maximum staleness should be greater than 100ms
     * @param no_thread whether NOT to invoke a background thread
     */
    Battery(const std::string &name, const std::chrono::milliseconds &max_staleness_ms, bool no_thread=false);
    
    /// the lock is neither copyable nor moveable
    Battery(const Battery &) = delete;

    /// the lock is neither copyable nor moveable
    Battery(Battery &&) = delete;
    
    /// the lock is neither copyable nor moveable
    Battery &operator=(const Battery &) = delete;

    /// the lock is neither copyable nor moveable
    Battery &operator=(Battery &&) = delete;
    
    virtual ~Battery();

//////////// Drivers: Implement the following functions
protected:
    /**
     * Refresh the battery status from the driver,
     * NOTE: Must update the battery status!!!
     * NOTE: Must update the timestamp!!! (in BatteryStatus)
     * NOTE: no need to check lock!!!
     * @return the new status of the battery
     */
    virtual BatteryStatus refresh() = 0;

    /**
     * Set the current of the battery
     * NOTE: no need to check lock!!!
     * @param target_current_mA the target current, target_current_mA > 0: discharging, target_current_mA < 0: charging
     * @param is_greater_than_target require whether the new current >= target_current_mA or <= target_current_mA
     */
    virtual uint32_t set_current(int64_t target_current_mA, bool is_greater_than_target) = 0;
public:
    /** 
     * (Optional function to override)
     * Possibly check the staleness and refresh the status of the battery if the staleness is expired. 
     * Or if automatic background refresh is enabled, just return the battery status. 
     * NOTE: Must add lock!!!!!!
     * @return the status of the battery, possibly new or a bit stale
     */
    virtual BatteryStatus get_status();
    
    /**
     * (Optional function to override)
     * Schedule to set the current to greater than / less than target current in a specific time. 
     * NOTE: Must add lock!!!!!!
     * @param target_current_mA the target current, target_current_mA > 0: discharging, target_current_mA < 0: charging
     * @param is_greater_than_target require whether the new current >= target_current_mA or <= target_current_mA
     * @param when_to_set a timestamp representing when to set the current to satisfy the request
     * @param until_when a timestamp representing when to stop setting the current
     * @return something? 
     */
    virtual uint32_t schedule_set_current(int64_t target_current_mA, bool is_greater_than_target, timepoint_t when_to_set, timepoint_t until_when);

    /**
     * Just return the type string of the battery driver
     * @return the string representing the type of the driver
     */
    virtual std::string get_type_string() {
        return "BatteryInterface";
    }
//////////// Up here

    /**
     * Manually refresh the battery status from the driver,
     * not recommended
     * @return the new status of the battery
     */
    BatteryStatus manual_refresh();

    /**
     * The name of the battery
     * @return the battery name
     */
    const std::string &get_name();

    /**
     * Update the maximum staleness
     * @param new_staleness_ms the new value to update the max_staleness, in milliseconds
     */
    void set_max_staleness(const std::chrono::milliseconds &new_staleness_ms);

    /**
     * The getter for max_staleness
     * @return the max_staleness
     */
    std::chrono::milliseconds get_max_staleness();

    ////// Staleness-based refresh related
    /**
     * In LAZY mode, check for staleness and then refresh if the status is too old, 
     * otherwise do nothing. 
     * @return if refresh is performed
     */
    bool check_staleness_and_refresh();

    ////// Background refresh related
    /**
     * The battery refreshing function, this should be the main function of background refreshing thread!
     * @param bat the pointer to the Battery class
     */
    static void background_func(Battery *bat);

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

    uint64_t next_sequence_number() {
        return (this->current_sequence_number++);
    }
    ////// Ends here

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

bool operator < (const Battery::event_t &lhs, const Battery::event_t &rhs);
bool operator > (const Battery::event_t &lhs, const Battery::event_t &rhs);

class PhysicalBattery : public Battery {
public: 
    PhysicalBattery(
        const std::string &name, 
        const std::chrono::milliseconds &max_staleness=std::chrono::milliseconds(1000)
    ) : Battery(name, max_staleness) {}
    std::string get_type_string() override {
        return "PhysicalBattery";
    }
};


class VirtualBattery : public Battery {
public: 
    VirtualBattery(
        const std::string &name, 
        const std::chrono::milliseconds &max_staleness=std::chrono::milliseconds(1000)
    ) : Battery(name, max_staleness) {}
    std::string get_type_string() override {
        return "VirtualBattery";
    }
};


#endif // ! BATTERY_INTERFACE_HPP





























