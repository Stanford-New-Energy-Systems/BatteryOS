#include "Driver.hpp"

Driver::~Driver() {};

Driver::Driver(int identifier) {
    this->identifier = identifier;
}

BatteryStatus Driver::refresh() {
    PRINT() << "DRIVER REFRESH!!!!" << std::endl;

    BatteryStatus status;
    status.voltage_mV = 5;
    status.current_mA = 0;
    status.capacity_mAh = 10;
    status.max_capacity_mAh = 10;
    status.max_charging_current_mA = 1200;
    status.max_discharging_current_mA = 1200;
    return status; 
}

bool Driver::set_current(double current_mA) {
    PRINT() << "DRIVER SET CURRENT: " << current_mA << "mA" << std::endl;
    return true; 
}

void* CreateDriverBattery(void* args) {
    const char** initArgs = (const char**)args;
    return (void*) new Driver(atoi(initArgs[0])); 
}

void DestroyDriverBattery(void* battery) {
    Driver* bat = (Driver*) battery;
    delete bat;
}

BatteryStatus DriverRefresh(void* battery) {
    Driver* bat = (Driver*) battery;
    return bat->refresh();
}

bool DriverSetCurrent(void* battery, double current_mA) {
    Driver* bat = (Driver*) battery;
    return bat->set_current(current_mA);
}
