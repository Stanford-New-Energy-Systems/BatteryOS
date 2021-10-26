#include "util.hpp"

bool get_system_time_c(int64_t *sec_since_epoch, int64_t *msec) {
    struct timespec tp;
    if (clock_gettime(CLOCK_REALTIME, &tp) != 0) {
        fprintf(stderr, "get_time(): clock_gettime fails!!!");
        return false;
    }
    (*sec_since_epoch) = tp.tv_sec;
    (*msec) = round(tp.tv_nsec / 1.0e6);
    if ((*msec) > 999) {
        (*sec_since_epoch) += 1;
        (*msec) = 0;
    }
    return true;
}

timepoint_t get_system_time() {
    return std::chrono::system_clock::now();
}

void print_buffer(uint8_t *buffer, size_t buffer_size) {
    for (size_t i = 0; i < buffer_size; ++i) {
        printf("%x ", (int)(*(buffer + i)));
    }
}

std::ostream & operator <<(std::ostream &out, const BatteryStatus &status) {
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


