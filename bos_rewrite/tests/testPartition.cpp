#include "BatteryDirectory.hpp"
#include "PartitionBattery.hpp"
#include "AggregateBattery.hpp"
#include "PhysicalBattery.hpp"

int main() {
    using namespace std::chrono_literals;
    BatteryDirectory d;
    {
        std::shared_ptr<Battery> bat0 = std::make_shared<PhysicalBattery>("bat0", std::chrono::seconds(100));
        std::shared_ptr<Battery> bat1 = std::make_shared<PhysicalBattery>("bat1", std::chrono::seconds(100));

        std::this_thread::sleep_for(1s);
        
        BatteryStatus status;

        status.voltage_mV = 5;
        status.current_mA = 0;
        status.capacity_mAh = 7500;
        status.max_capacity_mAh = 7500;
        status.max_charging_current_mA = 3600;
        status.max_discharging_current_mA = 3600;
        
        bat0->setBatteryStatus(status);

        status.voltage_mV = 5;
        status.current_mA = 0;
        status.capacity_mAh = 10000;
        status.max_capacity_mAh = 10000;
        status.max_charging_current_mA = 7000;
        status.max_discharging_current_mA = 7000;

        bat1->setBatteryStatus(status);

        d.addBattery(bat0);
        d.addBattery(bat1);

        std::vector<std::shared_ptr<Battery>> parents;
        parents.push_back(bat0);
        parents.push_back(bat1);        

        std::shared_ptr<Battery> bat2 = std::make_shared<AggregateBattery>("bat2", parents);

        d.addBattery(bat2);

        d.addEdge("bat0", "bat2");
        d.addEdge("bat1", "bat2");

        std::shared_ptr<PartitionBattery> bat3 = std::make_shared<PartitionBattery>("bat3");
        std::shared_ptr<PartitionBattery> bat4 = std::make_shared<PartitionBattery>("bat4");

        d.addBattery(bat3);
        d.addBattery(bat4);

        std::vector<std::weak_ptr<VirtualBattery>> children;
        children.push_back(bat3);
        children.push_back(bat4);

        Scale s1(0.5, 0.5);
        Scale s2(0.5, 0.5);

        std::vector<Scale> proportions;
        proportions.push_back(s1);
        proportions.push_back(s2);

        std::shared_ptr<PartitionManager> pManager = std::make_shared<PartitionManager>("pManager", proportions, PolicyType::PROPORTIONAL, bat2, children);

        bat3->setSourceBattery(pManager);
        bat4->setSourceBattery(pManager);

        d.addBattery(pManager);

        d.addEdge("bat2", "pManager");
        d.addEdge("pManager", "bat3");       
        d.addEdge("pManager", "bat4");       

        std::vector<std::shared_ptr<Battery>> parents2;
        parents2.push_back(bat3);
        parents2.push_back(bat4);
        
        std::shared_ptr<Battery> bat5 = std::make_shared<AggregateBattery>("bat5", parents2, std::chrono::seconds(5), RefreshMode::ACTIVE);
    
        std::this_thread::sleep_for(1s);

//        PRINT() << bat5->getStatus() << std::endl;

        d.addBattery(bat5);
        d.addEdge("bat3", "bat5");
        d.addEdge("bat4", "bat5");

        std::shared_ptr<PartitionBattery> bat6 = std::make_shared<PartitionBattery>("bat6", std::chrono::seconds(100));
        std::shared_ptr<PartitionBattery> bat7 = std::make_shared<PartitionBattery>("bat7", std::chrono::seconds(100));

        d.addBattery(bat6);
        d.addBattery(bat7);

        std::vector<std::weak_ptr<VirtualBattery>> children2;
        children2.push_back(bat6);
        children2.push_back(bat7);
        
        std::shared_ptr<PartitionManager> pManager2 = std::make_shared<PartitionManager>("pManager2", proportions, PolicyType::PROPORTIONAL, bat5, children2);

        d.addBattery(pManager2);

        d.addEdge("bat5", "pManager2");
        d.addEdge("pManager2", "bat6");
        d.addEdge("pManager2", "bat7");

        bat6->setSourceBattery(pManager2);
        bat7->setSourceBattery(pManager2); 

        std::this_thread::sleep_for(5s);
        
        timepoint_t currentTime = getTimeNow();

        bat6->Battery::schedule_set_current(1750, currentTime+1s,  currentTime+20s);
        bat7->Battery::schedule_set_current(1750, currentTime+10s, currentTime+18s);
        bat6->Battery::schedule_set_current(1500, currentTime+5s, currentTime+15s);

        std::this_thread::sleep_for(20s);

//        PRINT() << bat6->getStatus() << std::endl;
//        std::this_thread::sleep_for(10s);
//        PRINT() << bat6->getStatus() << std::endl;
//        std::this_thread::sleep_for(10s);

//        d.destroyDirectory();
    }
    
        

    d.removeBattery("bat1");
    d.removeBattery("bat0");
  
    return 0;
}
