#include <stdio.h>
#include <stdlib.h>

#include "iec61850.hpp"

IEC61850::IEC61850(const std::string &name, std::chrono::milliseconds staleness, std::string LogicalDevice_Name, std::string ZBAT_Name, 
std::string ZBTC_Name, std::string ZINV_Name)
: PhysicalBattery(name, staleness) {
    this -> ZBAT_Name = ZBAT_Name;
    this -> ZBTC_Name = ZBTC_Name;
    this -> ZINV_Name = ZINV_Name;
    this -> LogicalDevice_Name = LogicalDevice_Name;
    create_iec61850_client("localhost", 102); // quit gracefully if this returns false
}

IEC61850::IEC61850(const std::string &name, std::chrono::milliseconds staleness, std::string LogicalDevice_Name, std::string ZBAT_Name, 
std::string ZBTC_Name, std::string ZINV_Name, std::string hostname, int tcpPort) : PhysicalBattery(name, staleness) {
    this -> ZBAT_Name = ZBAT_Name;
    this -> ZBTC_Name = ZBTC_Name;
    this -> ZINV_Name = ZINV_Name;
    this -> LogicalDevice_Name = LogicalDevice_Name;
    create_iec61850_client(hostname, tcpPort); // quit gracefully if this returns false
}

IEC61850::~IEC61850() {
    IedConnection_close(con);
	IedConnection_destroy(con);
}

std::string IEC61850::getBatteryString() const {
    return "IEC61850";
}

BatteryStatus IEC61850::refresh() {
    MmsValue* value;
    std::string voltage = LogicalDevice_Name + '/' + ZBAT_Name + ".Vol.mag.f";
    std::string current = LogicalDevice_Name + '/' + ZBAT_Name + ".Amp.mag.f";
    std::string max_capacity = LogicalDevice_Name + '/' + ZBAT_Name + ".AhrRtg.setMag.f";
    std::string discharging_current = LogicalDevice_Name + '/' + ZBAT_Name + ".MaxBatA.setMag.f";

    // Should I typecast the float to int64_t??
    value = IedConnection_readObject(con, &error, voltage.c_str(), IEC61850_FC_MX);
    if (check_MmsValue(value))
        status.voltage_mV = (int64_t) MmsValue_toFloat(value);
    
    value = IedConnection_readObject(con, &error, current.c_str(), IEC61850_FC_MX);
    if (check_MmsValue(value)) // Amp 
        status.current_mA = (int64_t) MmsValue_toFloat(value);

    value = IedConnection_readObject(con, &error, max_capacity.c_str(), IEC61850_FC_SP);
    if (check_MmsValue(value)) // AhrRtg // value should be non-volatile so should represent the max_capacity instead of current capcacity
        status.max_capacity_mAh =  (int64_t) MmsValue_toFloat(value);

    value = IedConnection_readObject(con, &error, discharging_current.c_str(), IEC61850_FC_SP);
    if (check_MmsValue(value)) // MaxBatA
        status.max_discharging_current_mA = (int64_t) MmsValue_toFloat(value);

    status.time = convertToMilliseconds(getTimeNow());

    status.max_charging_current_mA = 10;
    // status.capacity_mAh
    return status;
}

bool IEC61850::set_current(double current_mA) {
    // do I need to check staleness and refresh???
    // MmsValue* value;
    std::string charge_mode = LogicalDevice_Name + '/' + ZBTC_Name + ".BatChaMod.setVal";
    std::string current_limit = LogicalDevice_Name + '/' + ZINV_Name + ".InALim.setMag.i";
    std::string recharge_rate = LogicalDevice_Name + '/' + ZBTC_Name + ".ReChaRte.setMag.i";

    if ((current_mA < 0) && (-current_mA) < this->status.max_charging_current_mA) {
        int current = (-current_mA);

        IedConnection_writeObject(con, &error, recharge_rate.c_str(), IEC61850_FC_SP, MmsValue_newIntegerFromInt64(current));
        if (error != IED_ERROR_OK)
            LOG() << "Failed to write recharge rate: " + recharge_rate + " to server";;

        // switches battery to Operatinal Mode (turns it on??)
        IedConnection_writeObject(con, &error, charge_mode.c_str(), IEC61850_FC_SP, MmsValue_newIntegerFromInt64(2)); // Operational Mode page 84 of ZBAT file 
        if (error != IED_ERROR_OK)
            LOG() << "Failed to write charge mode to server";
        
        return 0;
    } else if (current_mA > 0 && current_mA < this->status.max_discharging_current_mA) {
        // Turn Battery Charger Off 
        IedConnection_writeObject(con, &error, charge_mode.c_str(), IEC61850_FC_SP, MmsValue_newIntegerFromInt64(1));
        if (error != IED_ERROR_OK) // How do you turn the inverter on/off?
            LOG() << "Failed to write charge mode to server";
        
        // Sets current limit for inverter
        IedConnection_writeObject(con, &error, current_limit.c_str(), IEC61850_FC_SP, MmsValue_newIntegerFromInt64(current_mA));
        if (error != IED_ERROR_OK)
            LOG() << "Failed to write current limit: " + current_limit + " to server"; 

        return 0;
    }
    return 1;
}

bool IEC61850::check_MmsValue(MmsValue* value) {
    if (value == NULL)
        return false;
    return true;
}

bool IEC61850::create_iec61850_client(std::string hostname, int tcpPort) {
    this->config = TLSConfiguration_create();
    TLSConfiguration_setClientMode(config);
    TLSConfiguration_setOwnCertificateFromFile(config, "../certs/client.pem"); 
    TLSConfiguration_setOwnKeyFromFile(config, "../certs/client.key", nullptr);
    TLSConfiguration_addCACertificateFromFile(config, "../certs/ca_cert.pem");

    this->con = IedConnection_createEx(config, false); 

    IedConnection_connect(con, &error, hostname.c_str(), tcpPort);

    if (error != IED_ERROR_OK) {
        IedConnection_close(con);
        IedConnection_destroy(con);
        TLSConfiguration_destroy(config);
        return false;
    }
    return true;
}
