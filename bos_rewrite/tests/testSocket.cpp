#include <thread>
#include "Admin.hpp"
#include "ClientBattery.hpp"
#include "SecureClientBattery.hpp"

using timepoint_t = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>;

timepoint_t getTime() {
    return std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()); 
}

template <typename T>
std::vector<T> createVector(std::initializer_list<T> list) {
    std::vector<T> vector;
    for (T elem : list) {
        vector.push_back(elem);
    }
    return vector;
}

std::vector<std::string> createVector(std::initializer_list<const char*> list) {
    std::vector<std::string> vector;
    for (const char* elem : list) {
        vector.push_back(std::string(elem));
    }
    return vector;
}

int main() {
    using namespace std::chrono_literals;

    Admin admin(65432);
    
    if (!admin.createPhysicalBattery("bat0", std::chrono::seconds(100)))
        ERROR() << "could not create bat0 battery!" << std::endl;
    if (!admin.createPhysicalBattery("bat1", std::chrono::seconds(100)))
        ERROR() << "could not create bat1 battery!" << std::endl;
    if (!admin.createPhysicalBattery("batFoo", std::chrono::seconds(100)))
        ERROR() << "could not create bat1 battery!" << std::endl;
    //if (!admin.createSecureBattery("batSec", 2))
    //    ERROR() << "could not create batSec battery!" << std::endl;

    // TODO: server receives some junk data when sleep is removed... debug this...
    //sleep(2);
    ClientBattery bat0(65431, "bat0");
    ClientBattery bat1(65431, "bat1");
    ClientBattery batFoo(65431, "batFoo");
    //SecureClientBattery batSec(65431, 65430, "batSec");

    //timepoint_t currentTime = getTime();
    //batSec.schedule_set_current(1, currentTime+1min,  currentTime+20min);
    //batSec.submit_schedule();

    //batSec.schedule_set_current(0, currentTime+1min,  currentTime+20min);
    //batSec.schedule_set_current(1, currentTime+40min,  currentTime+60min);
    //batSec.submit_schedule();

    BatteryStatus status;

    status.voltage_mV = 5;
    status.current_mA = 0;
    status.capacity_mAh = 7500;
    status.max_capacity_mAh = 7500;
    status.max_charging_current_mA = 3600;
    status.max_discharging_current_mA = 3600;

    std::cout << "SET BAT0 STATUS" << std::endl;
    if (!bat0.setBatteryStatus(status)) {
        ERROR() << "could not set bat0 battery status" << std::endl;
        exit(1);
    }

    status.voltage_mV = 5;
    status.current_mA = 0;
    status.capacity_mAh = 10000;
    status.max_capacity_mAh = 10000;
    status.max_charging_current_mA = 7000;
    status.max_discharging_current_mA = 7000;

    std::cout << "SET BAT1 STATUS" << std::endl;
    if (!bat1.setBatteryStatus(status)) {
        ERROR() << "could not set bat1 battery status" << std::endl;
        exit(1);
    }

    std::this_thread::sleep_for(1s);
    
    LOG() << "bat0 status" << std::endl;
    PRINT() << bat0.getStatus();
    LOG() << "bat1 status" << std::endl;
    PRINT() << bat1.getStatus();

    if (!admin.createAggregateBattery("bat2", createVector({"bat0", "bat1"})))
        ERROR() << "could not create aggregate battery: bat2" << std::endl;

    ClientBattery bat2(65431, "bat2");

    LOG() << "bat2 status" << std::endl;
    PRINT() << bat2.getStatus();

    if (!admin.createPartitionBattery("bat2", PolicyType::PROPORTIONAL, createVector({"bat3", "bat4"}), createVector({Scale(0.5,0.5), Scale(0.5, 0.5)})))
        ERROR() << "could not create partition batteries: bat3 and bat4" << std::endl;

    ClientBattery bat3(65431, "bat3");
    ClientBattery bat4(65431, "bat4");

    LOG() << "bat3 status" << std::endl;
    PRINT() << bat3.getStatus();
    LOG() << "bat4 status" << std::endl;
    PRINT() << bat4.getStatus();

    if (!admin.createAggregateBattery("bat5", createVector({"bat3", "bat4"}), std::chrono::seconds(5), RefreshMode::ACTIVE))
        ERROR() << "could not create aggregate battery bat5" << std::endl;

    ClientBattery bat5(65431, "bat5");

    LOG() << "bat5 status" << std::endl;
    PRINT() << bat5.getStatus();

    if (!admin.createPartitionBattery("bat5", PolicyType::PROPORTIONAL, createVector({"bat6", "bat7"}), createVector({Scale(0.5,0.5), Scale(0.5, 0.5)})))
        ERROR() << "could not create partition batteries: bat6 and bat7" << std::endl;

    ClientBattery bat6(65431, "bat6");
    ClientBattery bat7(65431, "bat7");

    LOG() << "bat6 status" << std::endl;
    PRINT() << bat6.getStatus();
    LOG() << "bat7 status" << std::endl;
    PRINT() << bat7.getStatus();

    timepoint_t currentTime = getTime();

    bat6.schedule_set_current(1750, currentTime+1s,  currentTime+20s);
    bat7.schedule_set_current(1750, currentTime+10s, currentTime+18s);
    bat6.schedule_set_current(1500, currentTime+5s, currentTime+15s);

    LOG() << "bat6 status" << std::endl;
    PRINT() << bat6.getStatus();
    LOG() << "bat7 status" << std::endl;
    PRINT() << bat7.getStatus();

    //std::this_thread::sleep_for(20s);

    admin.shutdown();
    return 0;
}
