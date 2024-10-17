#include "BatteryStatus.hpp"

timestamp_t getTimeNow(void) {
    return std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()); 
}

char* formatTime(uint64_t time) {
    std::time_t t = std::chrono::system_clock::to_time_t(convertToTimestamp(time));
    return std::ctime(&t);
} 

bool checkIfZero(const BatteryStatus& status) {
    if (status.voltage_mV == 0 &&
        status.current_mA == 0 &&   
        status.capacity_mAh == 0 &&   
        status.max_capacity_mAh == 0 &&   
        status.max_charging_current_mA == 0 &&   
        status.max_discharging_current_mA == 0) 
        return true;
    return false;   
}

uint64_t convertToMilliseconds(timestamp_t time) {
    return time.time_since_epoch().count();
}

timestamp_t convertToTimestamp(uint64_t milliseconds) {
    timestamp_t ts;
    std::chrono::milliseconds t = std::chrono::milliseconds(milliseconds);
    ts += t;
    return ts;
}

void BatteryStatus::toProto(bosproto::BatteryStatus& proto_status) const {
    proto_status.set_voltage_mv(this->voltage_mV);
    proto_status.set_current_ma(this->current_mA);
    proto_status.set_capacity_mah(this->capacity_mAh);
    proto_status.set_max_capacity_mah(this->max_capacity_mAh);
    proto_status.set_max_charging_current_ma(this->max_charging_current_mA);
    proto_status.set_max_discharging_current_ma(this->max_discharging_current_mA);
    proto_status.set_timestamp(this->time);
}

bool operator==(const BatteryStatus &lhs, const BatteryStatus &rhs) {
    return (lhs.voltage_mV == rhs.voltage_mV) &&
           (lhs.current_mA == rhs.current_mA) &&
           (lhs.capacity_mAh == rhs.capacity_mAh) &&
           (lhs.max_capacity_mAh == rhs.max_capacity_mAh) &&
           (lhs.max_charging_current_mA == rhs.max_charging_current_mA) &&
           (lhs.max_discharging_current_mA == rhs.max_discharging_current_mA);
}

std::ostream& operator<<(std::ostream &out, const BatteryStatus &status){
    out << "BatteryStatus status = {\n";
    out << "    .voltage_mV =                 " << status.voltage_mV                 << "mV,  \n";
    out << "    .current_mA =                 " << status.current_mA                 << "mA,  \n";
    out << "    .capacity_mAh =               " << status.capacity_mAh               << "mAh, \n";
    out << "    .max_capacity_mAh =           " << status.max_capacity_mAh           << "mAh, \n";
    out << "    .max_charging_current_mA =    " << status.max_charging_current_mA    << "mA,  \n";
    out << "    .max_discharging_current_mA = " << status.max_discharging_current_mA << "mA,  \n";
    out << "    .Latest Refresh =             " << formatTime(status.time)           << "     \n";
    out << "};\n";
    return out;
}
