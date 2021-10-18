#pragma once
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
    /// name of the battery
    const std::string name;

    /// the thread that refreshes the battery for a given sampling period 
    std::unique_ptr<std::thread> background_refresh_thread;
    /// the thread lock for the battery
    std::recursive_mutex lock; 
    using lockguard_t = std::lock_guard<std::recursive_mutex>;

    /// the status of the battery
    BatteryStatus status;

    /// the timestamp of last refresh
    timepoint_t timestamp;

    /// the wait time in ms between two refreshes if automatic refresh is enabled
    const int64_t sampling_period;

    /// estimated net charge of this battery. This must be initialized by BOS
    int32_t estimated_soc;  

    /// is it doing background refresh?
    bool should_background_refresh;
    /// tell the background refreshing thread that it should quit
    bool should_cancel_background_refresh;

public: 
    Battery(const std::string &name, const int64_t sampling_period=1000) : 
        name(name), 
        background_refresh_thread(nullptr),
        lock(),
        status(),
        timestamp(Util::get_system_time()),  // = bos.util.bos_time()
        sampling_period(sampling_period),
        estimated_soc(),
        should_background_refresh(false),
        should_cancel_background_refresh(false) {}

    virtual ~Battery() {
        this->stop_background_refresh();
    }

protected:
    /**
     * Refresh the battery status from the driver,
     * NOTE: also should update the battery status!!!
     * NOTE: also should update the timestamp!!!
     * NOTE: no need to check lock!!!
     * @return the new status of the battery
     */
    virtual BatteryStatus refresh() = 0;

public:
    /**
     * Manually refresh the battery status from the driver,
     * @return the new status of the battery
     */
    virtual BatteryStatus manual_refresh() {
        lockguard_t lkd(this->lock);
        return this->refresh();
    }

    /**
     * Set the current to greater than / less than target current in a specific time. 
     * NOTE: Must check should_background_refresh and add lock!!!!!!
     * @param target_current_mA the target current, target_current_mA > 0: discharging, target_current_mA < 0: charging
     * @param is_greater_than_target require whether the new current >= target_current_mA or <= target_current_mA
     * @param when_to_set a timestamp representing when to set the current to satisfy the request
     * @return something? 
     */
    virtual uint32_t set_current(int64_t target_current_mA, bool is_greater_than_target, timepoint_t when_to_set) = 0;

    /** 
     * Possibly check the staleness and refresh the status of the battery if the staleness is expired. 
     * 
     * NOTE: Must check should_background_refresh and add lock!!!!!!
     * @return the status of the battery, possibly new or a bit stale
     */
    virtual BatteryStatus get_status() = 0;

    /**
     * Just return the type string of the battery driver
     * @return the string representing the type of the driver
     */
    virtual std::string get_type_string() {
        return "BatteryInterface";
    }

    /**
     * The name of the battery
     * @return the battery name
     */
    const std::string &get_name() {
        return this->name;
    }

    /**
     * The battery refreshing function, this should be the main function of background refreshing thread!
     * @param bat the pointer to the Battery class
     */
    static void background_refresh_func(Battery *bat) {
        bool should_quit = false;
        int64_t sampling_period = 0;
        while (true) {
            bat->lock.lock();
            should_quit = bat->should_cancel_background_refresh;
            sampling_period = bat->sampling_period;
            if (should_quit) {
                bat->lock.unlock();
                return;
            } 
            bat->status = bat->refresh();
            bat->lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(sampling_period));
        }
        return;
    }

    /**
     * Start the background refreshing mechanism
     * This spawns a refreshing thread and creates a mutex for protection
     * @return whether the start is successful or not
     */
    bool start_background_refresh() {
        if (this->sampling_period <= 0) {
            return false;
        } else if (this->background_refresh_thread != nullptr) {
            return false;
        } else {
            this->should_background_refresh = true;
            this->background_refresh_thread = std::make_unique<std::thread>(Battery::background_refresh_func, this);
        }
        return true;
    }

    /**
     * Stop the background refresh and clean up the thread and lock
     * @return whether the stop is successful or not
     */
    bool stop_background_refresh() {
        if (this->should_background_refresh) {
            // ask the refreshing thread to quit
            this->lock.lock();
            this->should_cancel_background_refresh = true;
            this->lock.unlock();
            
            // now wait for the refreshing thread to join
            this->background_refresh_thread->join();
            
            // reset the pointers
            this->background_refresh_thread.reset();
            this->should_background_refresh = false;
            this->should_cancel_background_refresh = false;

            return true;
        }
        return false;
    }

    /**
     * Approximate the total charge that has passed through the battery. 
     * This function is general enough to work for background refresh() updates, which will call 
     * this with `end_current == new_current`, as well as set_current() requests, which will generally
     * call this with `end_current != new_current`.
     * @param end_current the current that the battery has just been measured at (before modification).
     * @param new_current the current that the battery has just been set to (after modification).
     */
    void update_estimated_soc(int32_t end_current, int32_t new_current) {
        lockguard_t lkg(this->lock);
        int32_t begin_current = this->status.current_mA;

        timepoint_t end_time = std::chrono::system_clock::now();
        // the microseconds elapsed!
        std::chrono::duration<int64_t, std::chrono::system_clock::period> duration = end_time - this->timestamp;
        int64_t periods = duration.count();  // on macOS, this is 1usec;
        int64_t msec = periods / (std::chrono::system_clock::period::den / 1000);  // on macOS, period::den / 1000 = 1000, this converts usec to msec
        double hours_elapsed = static_cast<double>(msec) / (3600 * 1000);

        int32_t estimated_mAh = ((begin_current + end_current) / 2) * hours_elapsed;
        
        this->estimated_soc -= estimated_mAh;

        this->timestamp = end_time;
    }

    /** 
     * Getter of estimated soc 
     * @return estimated soc
     */
    int32_t get_estimated_soc() {
        lockguard_t lkg(this->lock);
        uint32_t esoc = this->estimated_soc;
        return esoc;
    }
    /** 
     * Setter of estimated soc 
     * @param new_estimated_soc the new value 
     */
    void set_estimated_soc(int32_t new_estimated_soc) {
        lockguard_t lkg(this->lock);
        this->estimated_soc = new_estimated_soc;
        return;
    }

    /**
     * Resets the estimated soc to the state of charge in status
     */
    void reset_estimated_soc() {
        lockguard_t lkg(this->lock); 
        // note: get_status() also requires the lock, so we need recursive mutex
        this->estimated_soc = this->get_status().state_of_charge_mAh;
    }

};


class PhysicalBattery : public Battery {
public: 
    PhysicalBattery(const std::string &name, const int64_t sample_period=-1) : Battery(name, sample_period) {}
    std::string get_type_string() override {
        return "PhysicalBattery";
    }
};

