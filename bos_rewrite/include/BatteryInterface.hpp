#ifndef BATTERY_INTERFACE_HPP
#define BATTERY_INTERFACE_HPP

#include "util.hpp"
#include "node.hpp"
#include "refresh.hpp"
#include "event_t.hpp"
#include "BatteryStatus.hpp"

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <tuple>
#include <memory>
#include <utility>
#include <chrono>
#include <condition_variable>

/* global sequence number for events */
extern uint64_t SEQUENCE_NUMBER;
uint64_t getSequenceNumber(void);

/* get current system clock time */
timepoint_t getTimeNow(void);

using lock_t      = std::mutex;
using lockguard_t = std::lock_guard<lock_t>;

/**
* Abstract Battery Class
* @param lock:                  battery lock used between main and background threads
* @param status:                status of the battery
* @param eventSet:              set used to order and hold events to be processed
* @param eventMap:              map used to hold all set_current_* events indexed by sequence number (used for merging events)
* @param current_mA:            current of the battery
* @param quitThread:            signals to background thread that it should quit
* @param refreshMode:           refresh mode of the battery (either ACTIVE or LAZY)
* @param eventThread:           background thread for handling events in the eventSet
* @param batteryName:           name of the battery (unique for each Battery instance)
* @param maxStaleness:          time between two refreshes RefreshMode::ACTIVE;
                                max staleness tolerance for RefreshMODE::LAZY
* @param condition_variable:    condition variable used for event scheduling
*/
class Battery : public Node {
    protected:
        lock_t lock;
        bool quitThread;
        double current_mA;
        EventSet eventSet;
        EventMap eventMap;
        BatteryStatus status{};
        RefreshMode refreshMode;
        std::thread eventThread;
        const std::string batteryName;
        std::chrono::milliseconds maxStaleness;
        std::condition_variable condition_variable;                
    
    /**
     * Constructors
     *  - Delete copy constructor and copy assignment
     *  - Delete move constructor and move assignment 
     *  - Constructor should include battery name and max_current_mA (optional: max staleness, refresh mode)
     */
    public:
        virtual ~Battery();
        Battery(Battery&&) = delete;
        Battery(const Battery&) = delete;
        Battery& operator=(Battery &&) = delete;
        Battery& operator=(const Battery&) = delete;
        Battery(const std::string &batteryName,
                const std::chrono::milliseconds &maxStaleness = std::chrono::milliseconds(1000), 
                const RefreshMode &refreshMode = RefreshMode::LAZY);
    
    /**
     * Device Driver Functions (used by physical batteries)
     * @func refresh():     retrieves current status of battery
     * @func set_current(): takes target current as parameter and sets the current of the battery
     */
    protected:
        virtual BatteryStatus refresh() = 0;
        virtual bool set_current(double current_mA) = 0;
    
    /**
     * BAL API Functions (used by virtual batteries)
     * @func getStatus():            returns the current status of the logical battery
     * @func schedule_set_current(): specifies a set_current request with a startTime and endTime for request 
     */
    public:
        virtual BatteryStatus getStatus();
        bool schedule_set_current(double current_mA, uint64_t startTime, uint64_t endTime);
        bool schedule_set_current(double current_mA, timepoint_t startTime, timepoint_t endTime);
        virtual bool schedule_set_current(double current_mA, timepoint_t startTime, timepoint_t endTime, std::string name, uint64_t sequenceNumber);
    
    /**
     * Extra Protected Helper Functions
     * @func checkAndRefresh(): calls refresh() if last time battery was refreshed was after maxStaleness (for RefreshMode::LAZY)
     * @func runEventThread():  runs eventThread that handles events in eventSet 
     * @func checkMergeAndInsertEvents(): inserts set_current_* events into eventSet and merges together events if possible
     */
    protected:
        virtual void runEventThread();
        BatteryStatus checkAndRefresh();
        void checkMergeAndInsertEvents(std::string batteryName, double current_mA, timepoint_t startTime, timepoint_t endTime, uint64_t sequenceNumber);
    
    /**
     * Extra Public Helper Functions
     * @func quit():                     signals to eventThread to quit
     * @func getCurrent():               returns current of battery at moment function is called
     * @func getMaxStaleness():          returns maxStaleness
     * @func getMaxChargingCurrent():    returns max charging current of battery
     * @func getMaxDischargingCurrent(): returns max discharging current of battery
     * @func getBatteryName():           returns batteryName
     * @func getBatteryString():         returns if battery is a physical or virtual battery
     * @func cancelEvent():              cancels set current event
     * @func setMaxStaleness():          setter for maxStaleness
     * @func getDelay():                 calculates delay from setting battery from old_current_mA to new_current_mA 
     */
    public:
        void quit();
        double getCurrent() const;
        std::string getBatteryName() const;
        double getMaxChargingCurrent() const;
        double getMaxDischargingCurrent() const;
        virtual std::string getBatteryString() const;       
        std::chrono::milliseconds getMaxStaleness() const;
        void setRefreshMode(const RefreshMode &refreshMode);
        // bool cancelEvent(timepoint_t startTime, timepoint_t endTime);
        void setMaxStaleness(const std::chrono::milliseconds &maxStaleness);
        // virtual std::chrono::milliseconds getDelay(double old_current_mA, double new_current_mA) = 0;

    public:
        // helper function!! delete when done using 
        void setBatteryStatus(const BatteryStatus &status) {
            this->lock.lock();
            this->status = status;
            this->lock.unlock();
            return;
        }
};

#endif 
