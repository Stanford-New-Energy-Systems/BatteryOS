#ifndef FIFO_BATTERY_HPP
#define FIFO_BATTERY_HPP

#include <poll.h>
#include <fcntl.h>
#include <unistd.h>

#include "util.hpp"
#include "BatteryStatus.hpp"
#include "protobuf/battery.pb.h"
#include "protobuf/battery_manager.pb.h"

/**
 * FIFO Battery Class
 * 
 * This battery supports sending commands to the battery FIFOs
 * as well as reading the corresponding responses
 *
 * @param: inputFilePath:  file path for input FIFO (write commands to)
 * @param: outputFilePath: file path for output FIFO (read respnonses from)
 *
 * @func: getStatus:       serializes getStatus command to input FIFO and reads response from output FIFO
 * @func: schedule_set_current: serializes command to input FIFO and reads response from output FIFO 
**/
class FifoBattery {
    private:
        std::string inputFilePath;
        std::string outputFilePath;

    public:
        ~FifoBattery();
        FifoBattery(const std::string& inputFilePath, const std::string& outputFilePath);

    public:
        BatteryStatus getStatus();
        bool setBatteryStatus(const BatteryStatus& status); 
        bool schedule_set_current(double current_mA, uint64_t startTime, uint64_t endTime); 
        bool schedule_set_current(double current_mA, timestamp_t startTime, timestamp_t endTime);
};

#endif 
