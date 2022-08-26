#include "BatteryStatus.hpp"

/*******************
Timestamp Functions
********************/

Timestamp::~Timestamp() {};

Timestamp::Timestamp () {
    getCurrentTime();
}

Timestamp::Timestamp(uint64_t milliseconds) {
    this->time = convertToTimestamp(milliseconds);
}

void Timestamp::getCurrentTime() {
    this->time = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()); 
}

uint64_t Timestamp::getMilliseconds() const {
    return this->time.time_since_epoch().count();
}

timestamp_t Timestamp::convertToTimestamp(uint64_t milliseconds) {
    timestamp_t ts;
    std::chrono::milliseconds t = std::chrono::milliseconds(milliseconds);
    ts += t;
    return ts;
}

std::ostream& operator<<(std::ostream& out, const Timestamp& timestamp) {
    std::time_t t = std::chrono::system_clock::to_time_t(timestamp.time);
    out << std::ctime(&t);
    return out;
} 

/************************
Battery Status Functions
*************************/

BatteryStatus::~BatteryStatus() {};

BatteryStatus::BatteryStatus() {
    this->voltage_mV = 0;
    this->current_mA = 0;
    this->capacity_mAh = 0;
    this->max_capacity_mAh = 0;
    this->max_charging_current_mA = 0;
    this->max_discharging_current_mA = 0;
}

BatteryStatus::BatteryStatus(uint64_t milliseconds) {
    this->voltage_mV = 0;
    this->current_mA = 0;
    this->capacity_mAh = 0;
    this->max_capacity_mAh = 0;
    this->max_charging_current_mA = 0;
    this->max_discharging_current_mA = 0;
    this->timestamp = Timestamp(milliseconds);
}

bool BatteryStatus::checkIfZero() {
    return this->voltage_mV == 0 &&
           this->current_mA == 0 &&
           this->capacity_mAh == 0 &&
           this->max_capacity_mAh == 0 &&
           this->max_charging_current_mA == 0 &&
           this->max_discharging_current_mA == 0;
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
    out << "    .Latest Refresh =             " << status.timestamp << "\n";
    out << "};\n";
    return out;
}
