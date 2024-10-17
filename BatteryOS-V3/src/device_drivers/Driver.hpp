#ifndef DRIVER_HPP
#define DRIVER_HPP

#include <stdlib.h>
#include "util.hpp"
#include "BatteryStatus.hpp"

class Driver {
    private:
        int identifier;
   
    public:
        ~Driver();
        Driver(int identifier);

    public:
        BatteryStatus refresh();
        bool set_current(double current_mA);  
};

extern "C" void* CreateDriverBattery(void* args);
extern "C" void DestroyDriverBattery(void* battery);
extern "C" BatteryStatus DriverRefresh(void* battery);
extern "C" bool DriverSetCurrent(void* battery, double current_mA);

#endif
