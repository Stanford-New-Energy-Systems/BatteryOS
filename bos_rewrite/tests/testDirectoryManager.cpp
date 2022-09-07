#include "BatteryDirectoryManager.hpp"

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

    std::vector<std::shared_ptr<Battery>> batteries;
    std::unique_ptr<BatteryDirectoryManager> manager = std::make_unique<BatteryDirectoryManager>();    

    std::shared_ptr<Battery> bat0 = manager->createPhysicalBattery("bat0", std::chrono::seconds(100));
    std::shared_ptr<Battery> bat1 = manager->createPhysicalBattery("bat1", std::chrono::seconds(100));

    std::this_thread::sleep_for(1s);

    BatteryStatus status;

    status.voltage_mV = 5;
    status.current_mA = 0;
    status.capacity_mAh = 7500;
    status.max_capacity_mAh = 7500;
    status.max_charging_current_mA = 3600;
    status.max_discharging_current_mA = 3600;
    status.time = convertToMilliseconds(getTimeNow());

    bat0->setBatteryStatus(status);

    status.voltage_mV = 5;
    status.current_mA = 0;
    status.capacity_mAh = 10000;
    status.max_capacity_mAh = 10000;
    status.max_charging_current_mA = 7000;
    status.max_discharging_current_mA = 7000;
    status.time = convertToMilliseconds(getTimeNow());

    bat1->setBatteryStatus(status);

    std::shared_ptr<Battery> bat2 = manager->createAggregateBattery("bat2", createVector({"bat0", "bat1"})); 

    std::this_thread::sleep_for(1s);
    
    LOG() << "bat0 battery status" << std::endl;
    PRINT() << bat0->getStatus() << std::endl;

    LOG()  << "bat1 battery status" << std::endl;
    PRINT() << bat1->getStatus() << std::endl;

    batteries = manager->createPartitionBattery("bat2", PolicyType::PROPORTIONAL, createVector({"bat3", "bat4"}), createVector({Scale(0.5,0.5), Scale(0.5, 0.5)}));

    std::this_thread::sleep_for(1s);

    LOG()  << "bat2 battery status" << std::endl;
    PRINT() << bat2->getStatus() << std::endl;

    std::shared_ptr<Battery> bat3 = batteries[0];
    std::shared_ptr<Battery> bat4 = batteries[1];

    std::this_thread::sleep_for(1s);

    LOG()  << "bat3 battery status" << std::endl;
    PRINT() << bat3->getStatus() << std::endl;

    LOG()  << "bat4 battery status" << std::endl;
    PRINT() << bat4->getStatus() << std::endl;

    std::shared_ptr<Battery> bat5 = manager->createAggregateBattery("bat5", createVector({"bat3", "bat4"}), std::chrono::seconds(5), RefreshMode::ACTIVE);

    std::this_thread::sleep_for(1s);

    LOG()  << "bat5 battery status" << std::endl;
    PRINT() << bat5->getStatus() << std::endl;

    batteries = manager->createPartitionBattery("bat5", PolicyType::PROPORTIONAL, createVector({"bat6", "bat7"}), createVector({Scale(0.5,0.5), Scale(0.5, 0.5)}));

    std::shared_ptr<Battery> bat6 = batteries[0];
    std::shared_ptr<Battery> bat7 = batteries[1];

    std::this_thread::sleep_for(1s);

    LOG()  << "bat6 battery status" << std::endl;
    PRINT() << bat6->getStatus() << std::endl;

    LOG()  << "bat7 battery status" << std::endl;
    PRINT() << bat7->getStatus() << std::endl;

    timepoint_t currentTime = getTimeNow();

    bat6->Battery::schedule_set_current(1750, currentTime+1s,  currentTime+20s);
    bat7->Battery::schedule_set_current(1750, currentTime+10s, currentTime+18s);
    bat6->Battery::schedule_set_current(1500, currentTime+5s, currentTime+15s);

    std::this_thread::sleep_for(20s);

    manager->removeBattery("bat3");

    return 0;
}
