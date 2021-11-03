#include "JBDBMS.hpp"

JBDBMS::JBDBMS(
    const std::string &name, 
    const std::string &device_address, 
    const std::string &current_regulator_address,
    const std::chrono::milliseconds &max_staleness_ms
) : 
    PhysicalBattery(name, max_staleness_ms),
    pconnection(nullptr),
    rd6006(current_regulator_address),
    max_charging_current_mA(10000),         // 10A is enough? 
    max_discharging_current_mA(10000),      // 10A is enough?
    basic_state()
{
    // device_address example: /dev/ttyUSB0
    pconnection.reset(new UARTConnection(device_address));
    pconnection->connect();
    // send a byte to wake up the lazy BMS! 
    pconnection->write({'W', 'a', 'k', 'e'});
    pconnection->flush();  // flush

    if (!pconnection->is_connected()) {
        ERROR() << "failed to open connections!";
        // fprintf(stderr, "JBDBMS: failed to open connection!\n");
        // exit(1);
    }
    this->refresh();
}

uint16_t JBDBMS::compute_checksum(const std::vector<uint8_t> &payload) {
    uint16_t sum_of_payload = 0;
    for (uint8_t b : payload) {
        sum_of_payload += (uint16_t)b;  // overflow ignored
    }
    uint16_t checksum_value = (~sum_of_payload) + 1;
    return checksum_value;
}

std::vector<uint8_t> JBDBMS::read_register_command(uint8_t register_address) {
    std::vector<uint8_t> command = { starter_byte, read_indicator_byte };
    std::vector<uint8_t> payload = { register_address, 0x00 };
    uint16_t checksum = compute_checksum(payload);
    for (uint8_t b : payload) {
        command.push_back(b);
    }
    command.push_back(uint8_t((checksum >> 8) & 0xFF));
    command.push_back(uint8_t(checksum & 0xFF));

    command.push_back(ending_byte);
    return command;
}

std::vector<uint8_t> JBDBMS::write_register_command(uint8_t register_address, const std::vector<uint8_t> &bytes_to_write) {
    std::vector<uint8_t> command = { starter_byte, write_indicator_byte };
    std::vector<uint8_t> payload = { register_address, uint8_t(bytes_to_write.size() & 0xFF) };
    for (uint8_t b : bytes_to_write) {
        payload.push_back(b);
    }
    uint16_t checksum = compute_checksum(payload);
    for (uint8_t b : payload) {
        command.push_back(b);
    }
    command.push_back(uint8_t((checksum >> 8) & 0xFF));
    command.push_back(uint8_t(checksum & 0xFF));
    command.push_back(ending_byte);
    return command;
}

bool JBDBMS::verify_recv_checksum(const std::vector<uint8_t> &header, const std::vector<uint8_t> &data, uint16_t checksum, uint8_t ending_byte) {
    if (header.size() != 4) {
        WARNING() << ("header size != 4");
        // fprintf(stderr, "JBDBMS::verify_checksum: header size != 4\n");
        return false;
    }
    uint8_t starter = header[0];
    if (starter != 0xDD) {
        WARNING() << ("starter byte is not 0xdd");
        // fprintf(stderr, "JBDBMS::verify_checksum: starter byte is not 0xdd\n");
        return false;
    }
    if (ending_byte != 0x77) {
        WARNING() << ("ending byte is not 0x77");
        // fprintf(stderr, "JBDBMS::verify_checksum: ending byte is not 0x77\n");
        return false;
    }
    // uint8_t register_address = header[1];
    uint8_t command_status = header[2];
    uint8_t data_length = header[3];
    if (data.size() != (size_t)data_length) {
        WARNING() << "data length byte is " << data_length << ", but total data received is " << data.size();
        // fprintf(stderr, "JBDBMS::verify_checksum: data length byte is %u, but total data received is %u\n", (unsigned)data_length, (unsigned)data.size());
        return false;
    }
    std::vector<uint8_t> payload = {command_status, data_length};
    for (uint8_t b : data) {
        payload.push_back(b);
    }
    uint16_t cksum = compute_checksum(payload);
    if (cksum != checksum) {
        WARNING() << "checksum fails, computed checksum = " << cksum << ", but actual checksum = " << checksum;
        // fprintf(stderr, "JBDBMS::verify_checksum: checksum fails computed checksum = %u, but actual checksum = %u\n", (unsigned)cksum, (unsigned)checksum);
        return false;
    }
    
    return true;
}


std::vector<uint8_t> JBDBMS::query_info(const std::vector<uint8_t> &command) {
    constexpr int max_tries = 3;
    std::vector<uint8_t> header;
    std::vector<uint8_t> data;
    for (int i = 0; i < max_tries; ++i) {
        header.clear();
        data.clear();
        ssize_t bytes_written = pconnection->write(command);
        if (bytes_written < 0) {
            WARNING() << bytes_written << "bytes written!";
            // fprintf(stderr, "JBDBMS::query_info: Warning: %d bytes written!\n", (int)bytes_written);
            pconnection->flush();
            continue;
        }
        if ((size_t)bytes_written != command.size()) {
            WARNING() << "intended to write" << command.size() << " bytes, but " << bytes_written << " bytes written!";
            // fprintf(stderr, "JBDBMS::query_info: Warning: intended to write %d bytes, but %d bytes written!\n", (int)command.size(), (int)bytes_written);
            pconnection->flush();
            continue;
        }
        header = pconnection->read(4); // data header at least 4 bytes!
        if (header.size() != 4) {
            WARNING() << "data header invalid! Current trial: " << i+1;
            // fprintf(stderr, "JBDBMS::query_info: Warning: data header invalid! current trial: %d\n", i+1);
            pconnection->flush();
            continue;
        }

        int data_length = header[3];
        data = pconnection->read(data_length + 3);
        if (data.size() != unsigned(data_length + 3)) {
            WARNING() << "data invalid! Current trial: " << i+1;
            // fprintf(stderr, "JBDBMS::query_info: Warning: data invalid! current trial: %d\n", i+1);
            pconnection->flush();
            continue;
        }
        uint16_t checksum = (uint16_t(data[data.size() - 3]) << 8) | (uint16_t(data[data.size() - 2]) & 0xFF);
        uint8_t ending_byte = data[data.size() - 1];
        data.pop_back();
        data.pop_back();
        data.pop_back();
        
        if (!verify_recv_checksum(header, data, checksum, ending_byte)) {
            WARNING() << "checksum fails! Current trial: " << i+1;
            // fprintf(stderr, "JBDBMS::query_info: Warning: checksum fails! current trial: %d\n", i+1);
            pconnection->flush();
            continue;
        }
        break;
    }
    return data;
}

std::ostream & operator<<(std::ostream &out, const JBDBMS::State &s) {
    out << "State = {\n";
    out << "    voltage =           " << s.voltage_10mV << "*10mV, \n";
    out << "    current =           " << s.current_10mA << "*10mA, \n";
    out << "    remaining_charge =  " << s.remaining_charge_10mAh << "*10mAh, \n";
    out << "    max_capacity =      " << s.max_capacity_10mAh << "*10mAh, \n";
    out << "    cycles =            " << s.cycles << ", \n";
    out << "    manufacture =       " << s.manufacture << ", \n";
    out << "    balancing_bit_1 =   " << s.balancing_bit_1 << ", \n";
    out << "    balancing_bit_2 =   " << s.balancing_bit_2 << ", \n";
    out << "    protection_state =  " << s.protection_state << ", \n";
    out << "    version =           " << int(s.version) << ", \n";
    out << "    remaining_precent = " << int(s.remaining_percentage) << ", \n";
    out << "    MOSFET_status =     " << int(s.MOSFET_status) << ", \n"; 
    out << "    num_batteries =     " << int(s.num_batteries) << ", \n";
    out << "    num_temp_sensors =  " << int(s.num_temperature_sensors) << ", \n";
    out << "};\n";
    return out;
}

bool JBDBMS::calibrate(uint16_t max_capacity_10mAh, const std::vector<uint16_t> &voltages_mV) {
    if (voltages_mV.size() < 6) {
        return false;
    }
    this->query_info(write_register_command(enter_factory_mode_register, {enter_factory_mode_command[0], enter_factory_mode_command[1]}));
    this->query_info(write_register_command(0x10, {uint8_t((max_capacity_10mAh >> 8) & 0xFF), uint8_t(max_capacity_10mAh & 0xFF)}));

    std::array<uint8_t, 6> voltage_registers = {0x12, 0x32, 0x33, 0x34, 0x35, 0x13};
    
    uint16_t v = 0;
    for (size_t i = 0; i < voltage_registers.size(); ++i) {
        v = voltages_mV[i];
        this->query_info(write_register_command(voltage_registers[i], 
            {uint8_t((v >> 8) & 0xFF), uint8_t(v & 0xFF)}
        ));
    }
    this->query_info(write_register_command(exit_factory_mode_register, {exit_and_save_factory_mode_command[0], exit_and_save_factory_mode_command[1]}));
    return true;
}

// static std::tuple<bool, uint8_t, std::vector<uint8_t>> JBDBMS::extract_received_data(const std::vector<uint8_t> &data) {
//     std::vector<uint8_t> result;
//     if (data.size() < 7) return std::make_tuple(false, 0, std::move(result));
//     uint8_t register_address = data[1];
//     bool success = (data[2] == 0x00);
//     unsigned length = static_cast<unsigned>(data[3]);
//     for (unsigned i = 0; i < length; ++i) {
//         result.push_back(data[i + 4]);
//     }
//     return std::make_tuple(success, register_address, std::move(result));
// }


JBDBMS::State JBDBMS::get_basic_info() {
    // uint16_t voltage;
    // uint16_t current;
    // uint16_t remaining_charge;
    // uint16_t max_capacity;
    // uint16_t cycles;
    // uint16_t manufacture;
    // uint16_t balancing_bit_1;
    // uint16_t balancing_bit_2;
    // uint16_t protection_state;
    // uint8_t version;
    // uint8_t remaining_percentage;
    // uint8_t MOSFET_status;
    // uint8_t num_batteries;
    // uint8_t num_temperature_sensors;
    constexpr size_t H = 0;  // number of bytes preceding the actual data
    std::vector<uint8_t> info = query_info(read_register_command(0x03));
    basic_state.voltage_10mV =           (((uint16_t)info[H   ]) << 8) | (((uint16_t)info[H+1]) & 0xFF);
    basic_state.current_10mA =           (((int16_t)info[H+ 2]) << 8)  | (((int16_t)info[H+3]) & 0xFF);
    basic_state.remaining_charge_10mAh = (((uint16_t)info[H+ 4]) << 8) | (((uint16_t)info[H+5]) & 0xFF);
    basic_state.max_capacity_10mAh =     (((uint16_t)info[H+ 6]) << 8) | (((uint16_t)info[H+7]) & 0xFF);
    basic_state.cycles =                 (((uint16_t)info[H+ 8]) << 8) | (((uint16_t)info[H+9]) & 0xFF);
    basic_state.manufacture =            (((uint16_t)info[H+10]) << 8) | (((uint16_t)info[H+11]) & 0xFF);
    basic_state.balancing_bit_1 =        (((uint16_t)info[H+12]) << 8) | (((uint16_t)info[H+13]) & 0xFF);
    basic_state.balancing_bit_2 =        (((uint16_t)info[H+14]) << 8) | (((uint16_t)info[H+15]) & 0xFF);
    basic_state.protection_state =       (((uint16_t)info[H+16]) << 8) | (((uint16_t)info[H+17]) & 0xFF);
    basic_state.version =                info[H+18];
    basic_state.remaining_percentage =   info[H+19];
    basic_state.MOSFET_status =          info[H+20];
    basic_state.num_batteries =          info[H+21];
    basic_state.num_temperature_sensors = info[H+22];
    return basic_state;
}




BatteryStatus JBDBMS::refresh() {
    this->get_basic_info();
    status.voltage_mV =                   basic_state.voltage_10mV * 10;
    status.current_mA =                   basic_state.current_10mA * 10;
    status.state_of_charge_mAh =          basic_state.remaining_charge_10mAh * 10;
    status.max_capacity_mAh =             basic_state.max_capacity_10mAh * 10;
    status.max_charging_current_mA =      this->max_charging_current_mA;
    status.max_discharging_current_mA =   this->max_discharging_current_mA;
    status.timestamp = get_system_time_c();
    return status;
}


std::string JBDBMS::get_type_string() {
    return "JBDBMS";
}

// BatteryStatus JBDBMS::get_status() {
//     lockguard_t lkd(this->lock);
//     check_staleness_and_refresh();
//     return status;
// }


/// > 0 discharging, < 0 charging
uint32_t JBDBMS::set_current(int64_t target_current_mA, bool is_greater_than_target) {
    this->check_staleness_and_refresh();
    if (target_current_mA < 0 && (-target_current_mA) > this->max_charging_current_mA) {
        return 0;
    }
    if (target_current_mA > 0 && target_current_mA > this->max_discharging_current_mA) {
        return 0;
    }
    // over discharge
    if (target_current_mA > 0 && this->status.state_of_charge_mAh <= 5) {
        return 0;
    }
    // over charge 
    if (target_current_mA < 0 && this->status.state_of_charge_mAh + 5 >= this->status.max_capacity_mAh) {
        return 0;
    }

    rd6006.set_current_Amps(double(target_current_mA) / 1000);
    return 1;
}




