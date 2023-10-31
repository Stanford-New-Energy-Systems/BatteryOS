#include <string>
#include <iostream>

#include "battery.pb.h"
#include "battery_manager.pb.h"

int main() {
    std::string byte_string;    
    bosproto::BatteryStatus status;
    
    status.set_voltage_mv(5);
    status.set_current_ma(0);
    status.set_capacity_mah(7500);
    status.set_max_capacity_mah(7500);
    status.set_max_charging_current_ma(3600);
    status.set_max_discharging_current_ma(3600);
    status.set_timestamp(10000);

    status.SerializeToString(&byte_string);


    bosproto::BatteryStatus s;

    s.ParseFromString(byte_string);

    std::cout << s.voltage_mv() << std::endl;
    std::cout << s.current_ma() << std::endl;
    std::cout << s.capacity_mah() << std::endl;
    std::cout << s.max_capacity_mah() << std::endl;
    std::cout << s.max_charging_current_ma() << std::endl;
    std::cout << s.max_discharging_current_ma() << std::endl;
    std::cout << s.timestamp() << std::endl;

    std::cout << bosproto::Command::Schedule_Set_Current << std::endl;

    bosproto::Aggregate_Battery bat;

    bat.set_batteryname("bat0");
    bat.add_parentnames("bat1"); 
    bat.add_parentnames("bat2"); 
    bat.set_max_staleness(1000);
    bat.set_refresh_mode(bosproto::Refresh::ACTIVE);

    bat.SerializeToString(&byte_string);

    
    bosproto::Aggregate_Battery b;

    b.ParseFromString(byte_string);

    std::cout << b.batteryname() << std::endl;
    
    for (int i = 0; i < b.parentnames_size(); i++)
        std::cout << b.parentnames(i) << std::endl; 

    std::cout << b.max_staleness() << std::endl;
    std::cout << b.refresh_mode() << std::endl;

    return 0;
}
