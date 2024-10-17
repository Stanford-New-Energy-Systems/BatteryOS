#include "VirtualBattery.hpp"


VirtualBattery::~VirtualBattery() {
    PRINT() << "VIRTUAL DESTRUCTOR for " << getBatteryName() << std::endl;
}; 

VirtualBattery::VirtualBattery(const std::string &batteryName,
                               const std::chrono::milliseconds &maxStaleness,
                               const RefreshMode &refreshMode) : Battery(batteryName,
                                                                         maxStaleness,
                                                                         refreshMode) {};

std::string VirtualBattery::getBatteryString() const {
    return "VirtualBattery";
}

void VirtualBattery::setMaxChargingCurrent(double current_mA) {
    this->status.max_charging_current_mA = current_mA ;
}

void VirtualBattery::setMaxDischargingCurrent(double current_mA) {
    this->status.max_discharging_current_mA = current_mA ;
}

BatteryStatus VirtualBattery::refresh() {
    PRINT() << "VIRTUAL BATTERY REFRESH!!!!" << std::endl;

    BatteryStatus status;
    status.voltage_mV = 0;
    status.current_mA = 0;
    status.capacity_mAh = 0;
    status.max_capacity_mAh = 0;
    status.max_charging_current_mA = 0;
    status.max_discharging_current_mA = 0;
    status.time = convertToMilliseconds(getTimeNow());
    return status; 
}

bool VirtualBattery::set_current(double current_mA) {
    return true;
} 
