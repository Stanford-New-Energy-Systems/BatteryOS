#include "PhysicalBattery.hpp"

PhysicalBattery::~PhysicalBattery() {
    PRINT() << "PHYSICAL DESTRUCTOR" << std::endl;
    if (!quitThread)
                quit();
} 
        
PhysicalBattery::PhysicalBattery(const std::string &batteryName,
                                 const std::chrono::milliseconds &maxStaleness,
                                 const RefreshMode &refreshMode) : Battery(batteryName,
                                                                           maxStaleness,
                                                                           refreshMode) 
{
    this->type = BatteryType::Physical;
    this->eventSet.insert(event_t(this->batteryName,
                          EventID::REFRESH,
                          0,
                          getTimeNow(),
                          getSequenceNumber()));
    this->eventThread = std::thread(&PhysicalBattery::runEventThread, this);
}
    
std::string PhysicalBattery::getBatteryString() const {
    return "PhysicalBattery";
}
    
    
BatteryStatus PhysicalBattery::refresh() {
    PRINT() << "REFRESH!!!!" << std::endl;

    return this->status; 
}

bool PhysicalBattery::set_current(double current_mA) {
    PRINT() << "SET CURRENT: " << current_mA << "mA" << std::endl;
    this->status.current_mA = current_mA;
    return true; 
}
