#include "JBDBMS.hpp"

/**********************
Constructor/Destructor
***********************/

JBDBMS::~JBDBMS() {
    serialClose(this->fd);
}

JBDBMS::JBDBMS(const std::string& device_path, int baud,
               double max_charge, double max_discharge) {
    this->fd = serialOpen(device_path.c_str(), baud);   
    
    if (this->fd == -2)
        ERROR() << "Invalid baud rate" << std::endl;
    else if (this->fd == -1)
        ERROR() << "could not open device path" << std::endl;

    this->max_charging_current_mA    = max_charge;
    this->max_discharging_current_mA = max_discharge;
}

/****************
Private Functions
*****************/

bool JBDBMS::validateData(uint8_t startByte, uint8_t errorByte, std::vector<uint8_t>& payload, uint16_t checkSum, uint8_t stopByte) {
    if (startByte != BMS_STARTBYTE)
        return false;
    else if (errorByte == 0x80)
        return false;
    else if (checkSum != this->checksum(payload))
        return false;
    else if (stopByte != BMS_STOPBYTE)
        return false;
    return true;
}

uint16_t JBDBMS::checksum(const std::vector<uint8_t>& payload) {
    uint16_t c = 0;

    for (const uint8_t p : payload) {
        c += (uint16_t) p;
    }

    return ~c + 1;
}

/***************
Public Functions
****************/

BatteryStatus JBDBMS::refresh() {
    PRINT() << "JBDBMS REFRESH!!!" << std::endl;
    using namespace std::chrono_literals;

    BatteryStatus status{};
    std::vector<uint8_t> data;

    auto U16 = [] (const uint8_t x, const uint8_t y) -> uint16_t {
        return ((uint16_t) x << 8) | ((uint16_t) y);
    };
    
    auto S16 = [] (const int8_t x, const int8_t y) -> int16_t {
        return ((int16_t) x << 8) | ((int16_t) y);
    };

    while (true) {
        serialFlush(this->fd);
        
        for (const uint8_t c: BMS_REFRESH)
            serialPutchar(this->fd, (unsigned char)c);

        std::this_thread::sleep_for(1s); // wait for response from BMS

        if (serialDataAvail(this->fd) > 0)
            break; 
    }

    if (serialDataAvail(this->fd) != BMS_RX_BASIC_INFO_SIZE)
        return status; 

    for (int i = 0; i < BMS_RX_BASIC_INFO_SIZE; i++)
        data.push_back((uint8_t) serialGetchar(this->fd));

    uint8_t startByte = data[0];
    uint8_t errorByte = data[2];
    uint8_t stopByte  = data[33];
    uint16_t checkSum = U16(data[31], data[32]);
    std::vector<uint8_t> payload = {data.begin() + 2, data.end() - 3}; 

    if (!validateData(startByte, errorByte, payload, checkSum, stopByte))
        return status;

    status.voltage_mV                 = U16(data[4], data[5])   * 10;
    status.current_mA                 = S16(data[6], data[7])   * 10;
    status.capacity_mAh               = U16(data[8], data[9])   * 10;
    status.max_capacity_mAh           = U16(data[10], data[11]) * 10; 
    status.max_charging_current_mA    = this->max_charging_current_mA;
    status.max_discharging_current_mA = this->max_discharging_current_mA;
    status.time = convertToMilliseconds(getTimeNow());
    
    PRINT() << status << std::endl;

    return status;
}

bool JBDBMS::set_current(double current_mA) {
    return true;
}

/**********
C Functions
***********/

void* CreateJBDBMS(void* args) {
    const char** initArgs = (const char**)args;
    
    int baud                = atoi(initArgs[1]);
    int max_charge          = atof(initArgs[2]);     
    int max_discharge       = atof(initArgs[3]);
    std::string device_path = initArgs[0];

    return (void *) new JBDBMS(device_path, baud, max_charge, max_discharge);
}

void DestroyJBDBMS(void* battery) {
    JBDBMS* bat = (JBDBMS*) battery;
    delete bat;
}

BatteryStatus JBDBMSRefresh(void* battery) {
    JBDBMS* bat = (JBDBMS*) battery;
    return bat->refresh();
}

bool JBDBMSSetCurrent(void* battery, double current_mA) {
    JBDBMS* bat = (JBDBMS*) battery;
    return bat->set_current(current_mA);
}
