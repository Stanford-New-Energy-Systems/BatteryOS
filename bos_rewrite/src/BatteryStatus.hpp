#ifndef BATTERY_STATUS_H
#define BATTERY_STATUS_H
#include <ctime>
#include <cmath>
#include <chrono>
#include <time.h>
#include <iostream>
#include <stdint.h>
#include <stdbool.h>

using timestamp_t = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>;

/**
 * Timestamp struct
 *
 * The timestamp records the time at which a Battery Status
 * has been recorded
 *
 * @param: time: time status is recorded
 * @func: getCurrentTime: gets the current system clock time
 * @func: getMilliseconds: converts time to milliseconds (from epoch) 
 * @func: convertToTimestamp: takes time in milliseconds and converts back to system time
**/

struct Timestamp {
    timestamp_t time;

    public:
        Timestamp();
        ~Timestamp();
        Timestamp(uint64_t milliseconds);
    
    public:
        void getCurrentTime();
        uint64_t getMilliseconds() const;
        timestamp_t convertToTimestamp(uint64_t milliseconds);
        
        friend std::ostream& operator<<(std::ostream& out, const Timestamp& timestamp);
};

/* 
    Status of a Battery
    Fields:
        voltage_mV:                   voltage
        current_mA:                   current
        capacity_mAh                  current state of charge (SOC) of the battery
        max_capacity_mAh:             max capacity of the battery 
        max_charging_current_mA:      max charging current of the battery
        max_discharging_current_mA:   max discharging current of the battery
*/

typedef struct BatteryStatus {
    double voltage_mV; 
    double current_mA;
    double capacity_mAh;
    double max_capacity_mAh;
    double max_charging_current_mA;
    double max_discharging_current_mA;
    struct Timestamp timestamp;
    
    public:
        BatteryStatus();
        ~BatteryStatus();
        BatteryStatus(uint64_t milliseconds);

    public:
        bool checkIfZero();
        friend bool operator==(const BatteryStatus &lhs, const BatteryStatus &rhs);
        friend std::ostream& operator<<(std::ostream& out, const BatteryStatus& status);
} BatteryStatus;



#endif
