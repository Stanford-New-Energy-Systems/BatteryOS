#include "DynamicBattery.hpp"

DynamicBattery::~DynamicBattery() {
    this->destructor(this->battery);
}

DynamicBattery::DynamicBattery(void* initArgs,
                               void* destructor,
                               void* constructor,
                               void* refreshFunc,
                               void* setCurrentFunc,
                               const std::string& batteryName,
                               const std::chrono::milliseconds& maxStaleness,
                               const RefreshMode& refreshMode) : PhysicalBattery(batteryName,
                                                                                 maxStaleness,
                                                                                 refreshMode)
{
    this->refreshFunc    = (refresh_t)refreshFunc;
    this->destructor     = (destruct_t)destructor;
    this->constructor    = (construct_t)constructor; 
    this->setCurrentFunc = (set_current_t)setCurrentFunc;

    this->battery = this->constructor(initArgs);
}

BatteryStatus DynamicBattery::refresh() {
    return refreshFunc(this->battery);
}

bool DynamicBattery::set_current(double current_mA) {
    return setCurrentFunc(this->battery, current_mA);
}
