#pragma once

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <sys/time.h>
#include <chrono>
#include <iostream>
#include "BatteryStatus.h"
using timepoint_t = std::chrono::time_point<std::chrono::system_clock>;

struct Util {
    static bool get_system_time_c(int64_t *sec_since_epoch, int64_t *msec) {
        struct timespec tp;
        if (clock_gettime(CLOCK_REALTIME, &tp) != 0) {
            fprintf(stderr, "Util.get_time(): clock_gettime fails!!!");
            return false;
        }
        (*sec_since_epoch) = tp.tv_sec;
        (*msec) = tp.tv_nsec / 1000000;
        return true;
    }

    static timepoint_t get_system_time() {
        return std::chrono::system_clock::now();
    }

    static void print_buffer(uint8_t *buffer, size_t buffer_size) {
        for (size_t i = 0; i < buffer_size; ++i) {
            printf("%x ", (int)(*(buffer + i)));
        }
    }

    template <typename T>
    static size_t serialize_int(T number, uint8_t *buffer, size_t buffer_size) {
        if (buffer_size < sizeof(T)) {
            return 0;
        }
        for (size_t i = 0; i < sizeof(T); ++i) {
            (*(buffer+i)) = (uint8_t)((number >> (i * 8)) & 0xFF);
        }
        return sizeof(T);
    }
    
    template <typename T>
    static T deserialize_int(const uint8_t *buffer) {
        T result = 0;
        for (size_t i = 0; i < sizeof(T); ++i) {
            result |= ( ( (T)(*(buffer+i)) ) << (i * 8) );
        }
        return result;
    }



    static std::ostream & print_battery_status_to_stream(std::ostream &out, const BatteryStatus &status) {
        out << "BatteryStatus status = {\n";
        out << "    .voltage_mV =                 " << status.voltage_mV << "mV, \n";
        out << "    .current_mA =                 " << status.current_mA << "mA, \n";
        out << "    .state_of_charge_mAh =        " << status.state_of_charge_mAh << "mAh, \n";
        out << "    .max_capacity_mAh =           " << status.max_capacity_mAh << "mAh, \n";
        out << "    .max_charging_current_mA =    " << status.max_charging_current_mA << "mA, \n";
        out << "    .max_discharging_current_mA = " << status.max_discharging_current_mA << "mA, \n";
        out << "};\n";
        return out;
    }













};


