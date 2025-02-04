#ifndef BATTERY_STATUS_HPP
#define BATTERY_STATUS_HPP
#include <ctime>
#include <cmath>
#include <chrono>
#include <time.h>
#include <iostream>
#include <stdint.h>
#include <stdbool.h>

#include "protobuf/battery.pb.h"

using timestamp_t = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>;

/* get current system clock time */
timestamp_t getTimeNow(void);

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BatteryStatus BatteryStatus;

/* 
    Status of a Battery
    Fields:
        voltage_mV:                   voltage
        current_mA:                   current
        capacity_mAh                  current state of charge (SOC) of the battery
        max_capacity_mAh:             max capacity of the battery 
        max_charging_current_mA:      max charging current of the battery
        max_discharging_current_mA:   max discharging current of the battery
        time:                         time the status was captured
*/

struct BatteryStatus {
    double voltage_mV;
    double current_mA;
    double capacity_mAh;
    double max_capacity_mAh;
    double max_charging_current_mA;
    double max_discharging_current_mA;
    uint64_t time;

    BatteryStatus()
        : voltage_mV(0),
          current_mA(0),
          capacity_mAh(0),
          max_capacity_mAh(0),
          max_charging_current_mA(0),
          max_discharging_current_mA(0),
          time(0) {}
    BatteryStatus(const bosproto::BatteryStatus &proto_status)
        : voltage_mV(proto_status.voltage_mv()),
          current_mA(proto_status.current_ma()),
          capacity_mAh(proto_status.capacity_mah()),
          max_capacity_mAh(proto_status.max_capacity_mah()),
          max_charging_current_mA(proto_status.max_charging_current_ma()),
          max_discharging_current_mA(proto_status.max_discharging_current_ma()),
          time(proto_status.timestamp()) {}
    void toProto(bosproto::BatteryStatus &proto_status) const;
}; 

#ifdef __cplusplus
}
#endif

char* formatTime(uint64_t time);
bool checkIfZero(const BatteryStatus& status);
uint64_t convertToMilliseconds(timestamp_t time);
timestamp_t convertToTimestamp(uint64_t milliseconds); 
bool operator==(const BatteryStatus &lhs, const BatteryStatus &rhs);
std::ostream& operator<<(std::ostream& out, const BatteryStatus& status);

#endif
