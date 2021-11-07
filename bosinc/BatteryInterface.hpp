#ifndef BATTERY_INTERFACE_HPP
#define BATTERY_INTERFACE_HPP
#include "BOSNode.hpp"
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
#include <set>

#include "util.hpp"

/**
 * A Battery abstract class
 */
class Battery : public BOSNode {
public:
    /**
     * The refresh mode of the battery
     * LAZY : refresh if the status is older than maximum staleness
     * ACTIVE : automatically refresh in background with a period of maximum staleness
     */
    enum class RefreshMode {
        LAZY, 
        ACTIVE,
    };
    /**
     * The event IDs for BAL methods 
     * NOTE: the order of the events matters!!! 
     * REFRESH : just call refresh; 
     * SET_CURRENT_END : indicating when a schedule_set_current event ends; 
     * SET_CURRENT : indicating when a schedule_set_current event begins; 
     * CANCEL_EVENT : cancel all the SET_CURRENT_* events in that specific timepoint, REFRESH events are not affected
     */
    enum class Function : int {
        REFRESH = 0,
        SET_CURRENT_END = 1, 
        SET_CURRENT = 2,
        CANCEL_EVENT = 3,  
    };

    /**
     * A struct representing a BAL event
     * @param timepoint at what time does this event happen
     * @param time_added when this event is added 
     * @param sequence_number the sequence number of this event 
     *  roughly represent when the event is queued up, 
     *  and used for ordering multiple events happening at the same time
     * @param func what kind of this event is 
     * @param current_mA if this is a SET_CURRENT_* event, what's the target current 
     * @param is_greater_than if this is a SET_CURRENT_* event, do you want it to be greater than target current or less than target current
     */
    struct event_t {
        timepoint_t timepoint;
        timepoint_t time_added; 
        uint64_t sequence_number;
        Function func;
        int64_t current_mA;
        bool is_greater_than;
        void *other_data;
        event_t(
            timepoint_t tp, 
            uint64_t seq_num, 
            Function fn, 
            int64_t mA, 
            bool is_greater_than,
            void *other_data = nullptr
        ) : 
            timepoint(tp),
            time_added(get_system_time()),
            sequence_number(seq_num),
            func(fn),
            current_mA(mA),
            is_greater_than(is_greater_than),
            other_data(other_data) {}
        event_t() {}
    };

    typedef std::set<event_t> EventQueue;

protected: 
    /** name of the battery */
    const std::string name;

    /** the status of the battery */
    BatteryStatus status;

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

    /** the queue holding the events */
    EventQueue event_queue;

    uint64_t current_sequence_number;

    /** the requested current right now */
    int64_t current_now;
    bool is_greater_than_current_now;
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
    virtual uint32_t set_current(int64_t target_current_mA, bool is_greater_than_target, void *other_data) = 0;
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

    /** 
     * Just get the next sequence number for event 
     * @return the next sequence number
     */
    uint64_t next_sequence_number() {
        return (this->current_sequence_number++);
    }
    ////// Ends here
};
/**
 * Compare two event_t lexicographically 
 */
bool operator < (const Battery::event_t &lhs, const Battery::event_t &rhs);
/**
 * Compare two event_t lexicographically 
 */
bool operator > (const Battery::event_t &lhs, const Battery::event_t &rhs);

class PhysicalBattery : public Battery {
protected: 
public: 
    PhysicalBattery(
        const std::string &name, 
        const std::chrono::milliseconds &max_staleness=std::chrono::milliseconds(1000),
        bool no_thread = false
    ) : Battery(name, max_staleness, no_thread) {
        this->type = BatteryType::Physical;
    }
    std::string get_type_string() override {
        return "PhysicalBattery";
    }
};

class VirtualBattery : public Battery {
public: 
    VirtualBattery(
        const std::string &name, 
        const std::chrono::milliseconds &max_staleness=std::chrono::milliseconds(1000),
        bool no_thread = false
    ) : Battery(name, max_staleness, no_thread) {

    }
    std::string get_type_string() override {
        return "VirtualBattery";
    }

    void set_status(BatteryStatus target) {
        lockguard_t lkg(this->lock);
        this->status = target;
    }

    // this function is used to lock the local topology when BOSDirectory is trying to add an edge 
    void mutex_lock() {this->lock.lock();}
    // this function is used to lock the local topology when BOSDirectory is trying to add an edge 
    void mutex_unlock() {this->lock.unlock();}
    
};


#endif // ! BATTERY_INTERFACE_HPP



































