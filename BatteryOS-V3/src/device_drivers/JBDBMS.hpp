#ifndef JBDBMS_HPP
#define JBDBMS_HPP

#include <vector>
#include <thread>
#include <stdlib.h>
#include "util.hpp"
#include "wiringSerial.h"
#include "BatteryStatus.hpp"

// Constants
#define BMS_STARTBYTE 0xDD
#define BMS_STOPBYTE  0x77
#define BMS_READBYTE  0xA5
#define BMS_WRITEBYTE 0x5A

// Command codes (registers)
#define BMS_REG_BASIC_SYSTEM_INFO 0x03
#define BMS_REG_CELL_VOLTAGES     0x04
#define BMS_REG_NAME              0x05

// number of bytes sent from bms
// regarding basic info  
#define BMS_RX_BASIC_INFO_SIZE 34

constexpr uint8_t BMS_REFRESH[] {BMS_STARTBYTE,
                                 BMS_READBYTE,
                                 BMS_REG_BASIC_SYSTEM_INFO,
                                 0x00,
                                 0xff,
                                 0xfd,
                                 0x77};

class JBDBMS {
    private:
        int fd;
        double max_charging_current_mA;
        double max_discharging_current_mA;

    public:
        ~JBDBMS();
        JBDBMS(const std::string& device_path, int baud = 9600,
               double max_charge = 200000, double max_discharge = 200000);

    private:
        uint16_t checksum(const std::vector<uint8_t>& payload);
        bool validateData(uint8_t startByte, uint8_t errorByte, std::vector<uint8_t>& payload, uint16_t checkSum, uint8_t stopByte);

    public:
        BatteryStatus refresh();
        bool set_current(double current_mA);
};

extern "C" void* CreateJBDBMS(void* args);
extern "C" void DestroyJBDBMS(void* battery);
extern "C" BatteryStatus JBDBMSRefresh(void* battery);
extern "C" bool JBDBMSSetCurrent(void* battery, double current_mA);

#endif
