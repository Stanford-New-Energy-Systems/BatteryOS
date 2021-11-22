#include "util.hpp"

CTimestamp get_system_time_c() {
    struct timespec tp;
    if (clock_gettime(CLOCK_REALTIME, &tp) != 0) {
        WARNING() << ("clock_gettime fails!!!");
        // fprintf(stderr, "get_time(): clock_gettime fails!!!");
        return {0, 0};
    }
    CTimestamp timestamp;
    timestamp.secs_since_epoch = tp.tv_sec;
    timestamp.msec = round(tp.tv_nsec / 1.0e6);
    if (timestamp.msec > 999) {
        timestamp.secs_since_epoch += 1;
        timestamp.msec = 0;
    }
    return timestamp;
}

timepoint_t get_system_time() {
    return std::chrono::system_clock::now();
}

CTimestamp timepoint_to_c_time(const timepoint_t &tp) {
    auto secs = std::chrono::time_point_cast<std::chrono::seconds>(tp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp - secs);
    return {secs.time_since_epoch().count(), ms.count()};
    // auto duration_since_epoch = tp.time_since_epoch();
    // auto secs = std::chrono::duration_cast<std::chrono::seconds>(duration_since_epoch);
    // duration_since_epoch -= secs; 
    // auto msecs = std::chrono::duration_cast<std::chrono::milliseconds>(duration_since_epoch);
    // return {secs.count(), msecs.count()};
}


timepoint_t c_time_to_timepoint(const CTimestamp &timestamp) {
    timepoint_t tp;
    tp += std::chrono::seconds(timestamp.secs_since_epoch);
    tp += std::chrono::milliseconds(timestamp.msec);
    return tp;
}


void print_buffer(uint8_t *buffer, size_t buffer_size) {
    for (size_t i = 0; i < buffer_size; ++i) {
        printf("%x ", (int)(*(buffer + i)));
    }
}

std::ostream & operator <<(std::ostream &out, const BatteryStatus &status) {
    constexpr size_t tbuff_size = 20;
    char tbuff[tbuff_size];
    time_t now_ts = std::chrono::system_clock::to_time_t(c_time_to_timepoint(status.timestamp));
    strftime(tbuff, 20, "%Y-%m-%d %H:%M:%S", localtime(&now_ts));  // note: this is in local time
    tbuff[tbuff_size-1] = '\0';
    out << "BatteryStatus status = {\n";
    out << "    .voltage_mV =                 " << status.voltage_mV << "mV, \n";
    out << "    .current_mA =                 " << status.current_mA << "mA, \n";
    out << "    .capacity_mAh =               " << status.capacity_mAh << "mAh, \n";
    out << "    .max_capacity_mAh =           " << status.max_capacity_mAh << "mAh, \n";
    out << "    .max_charging_current_mA =    " << status.max_charging_current_mA << "mA, \n";
    out << "    .max_discharging_current_mA = " << status.max_discharging_current_mA << "mA, \n";
    out << "    .timestamp =                  " << tbuff << "\n";
    out << "};\n";
    return out;
}



void output_status_to_csv(CSVOutput &csv, const std::vector<BatteryStatus> &statuses) {
    constexpr size_t tbuff_size = 20;
    for (int i = 0; i < int(statuses.size()); ++i) {
        const BatteryStatus &status = statuses[i];
        char tbuff[tbuff_size];
        time_t now_ts = std::chrono::system_clock::to_time_t(c_time_to_timepoint(status.timestamp));
        strftime(tbuff, 20, "%Y-%m-%d %H:%M:%S", localtime(&now_ts));  // note: this is in local time
        tbuff[tbuff_size-1] = '\0';
        csv.stream() 
            << tbuff << ',' 
            << now_ts << ',' 
            << ((double)status.voltage_mV / 1000 * (double)status.current_mA / 1000)
            << ( (i < int(statuses.size() - 1)) ? ',' : '\n' );
    }
    csv.stream().flush();
}















