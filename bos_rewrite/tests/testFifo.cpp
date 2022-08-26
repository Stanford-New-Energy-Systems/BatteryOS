#include <thread>
#include "Admin.hpp"
#include "FifoBattery.hpp"

std::string path   = "batteries/";
std::string input  = "_fifo_input";
std::string output = "_fifo_output";

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

std::string makeInputFileName(const std::string& name) {
    return path + name + input;
}

std::string makeOutputFileName(const std::string& name) {
    return path + name + output;
}

int main() {
    using namespace std::chrono_literals;

    Admin admin(makeInputFileName("admin"), makeOutputFileName("admin"));

    if (!admin.createPhysicalBattery("bat0", std::chrono::seconds(100)))
        ERROR() << "could not create bat0 battery!" << std::endl;
    if (!admin.createPhysicalBattery("bat1", std::chrono::seconds(100)))
        ERROR() << "could not create bat1 battery!" << std::endl;
    
    FifoBattery bat0(makeInputFileName("bat0"), makeOutputFileName("bat0"));
    FifoBattery bat1(makeInputFileName("bat1"), makeOutputFileName("bat1"));
    
    BatteryStatus status;

    status.voltage_mV = 5;
    status.current_mA = 0;
    status.capacity_mAh = 7500;
    status.max_capacity_mAh = 7500;
    status.max_charging_current_mA = 3600;
    status.max_discharging_current_mA = 3600;

    if (!bat0.setBatteryStatus(status))
        ERROR() << "could not set bat0 battery status" << std::endl;

    status.voltage_mV = 5;
    status.current_mA = 0;
    status.capacity_mAh = 10000;
    status.max_capacity_mAh = 10000;
    status.max_charging_current_mA = 7000;
    status.max_discharging_current_mA = 7000;

    if (!bat1.setBatteryStatus(status))
        ERROR() << "could not set bat1 battery status" << std::endl;

    std::this_thread::sleep_for(1s);
    
    LOG() << "bat0 status" << std::endl;
    PRINT() << bat0.getStatus();
    LOG() << "bat1 status" << std::endl;
    PRINT() << bat1.getStatus();

    if (!admin.createAggregateBattery("bat2", createVector({"bat0", "bat1"})))
        ERROR() << "could not create aggregate battery: bat2" << std::endl;

    FifoBattery bat2(makeInputFileName("bat2"), makeOutputFileName("bat2"));

    LOG() << "bat2 status" << std::endl;
    PRINT() << bat2.getStatus();

    if (!admin.createPartitionBattery("bat2", PolicyType::PROPORTIONAL, createVector({"bat3", "bat4"}), createVector({Scale(0.5,0.5), Scale(0.5, 0.5)})))
        ERROR() << "could not create partition batteries: bat3 and bat4" << std::endl;

    FifoBattery bat3(makeInputFileName("bat3"), makeOutputFileName("bat3"));
    FifoBattery bat4(makeInputFileName("bat4"), makeOutputFileName("bat4"));

    LOG() << "bat3 status" << std::endl;
    PRINT() << bat3.getStatus();
    LOG() << "bat4 status" << std::endl;
    PRINT() << bat4.getStatus();

    if (!admin.createAggregateBattery("bat5", createVector({"bat3", "bat4"}), std::chrono::seconds(5), RefreshMode::ACTIVE))
        ERROR() << "could not create aggregate battery bat5" << std::endl;

    FifoBattery bat5(makeInputFileName("bat5"), makeOutputFileName("bat5"));

    LOG() << "bat5 status" << std::endl;
    PRINT() << bat5.getStatus();

    if (!admin.createPartitionBattery("bat5", PolicyType::PROPORTIONAL, createVector({"bat6", "bat7"}), createVector({Scale(0.5,0.5), Scale(0.5, 0.5)})))
        ERROR() << "could not create partition batteries: bat6 and bat7" << std::endl;

    FifoBattery bat6(makeInputFileName("bat6"), makeOutputFileName("bat6"));
    FifoBattery bat7(makeInputFileName("bat7"), makeOutputFileName("bat7"));

    LOG() << "bat6 status" << std::endl;
    PRINT() << bat6.getStatus();
    LOG() << "bat7 status" << std::endl;
    PRINT() << bat7.getStatus();

    timepoint_t currentTime = getTime();

    bat6.schedule_set_current(1750, currentTime+1s,  currentTime+20s);
    bat7.schedule_set_current(1750, currentTime+10s, currentTime+18s);
    bat6.schedule_set_current(1500, currentTime+5s, currentTime+15s);

    std::this_thread::sleep_for(20s);

    admin.shutdown();
    return 0;
}
