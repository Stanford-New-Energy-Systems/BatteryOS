#include "PhysicalBattery.hpp"
#include "AggregateBattery.hpp"
#include "BatteryDirectory.hpp"

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

        d.addBattery(bat0);
        d.addBattery(bat1);

        std::vector<std::shared_ptr<Battery>> parents;
        parents.push_back(bat0);
        parents.push_back(bat1);        

        std::shared_ptr<Battery> bat2 = std::make_shared<AggregateBattery>("bat2", parents);
        
        d.addBattery(bat2);
        
        d.addEdge("bat0", "bat2");
        d.addEdge("bat1", "bat2");

        LOG() << "bat2 status";
        PRINT() << bat2->getStatus() << std::endl;
        
        timepoint_t currentTime = getTimeNow();

        bat2->schedule_set_current(8400, currentTime+1s, currentTime+6s);
        bat1->schedule_set_current(-4800, currentTime+2s, currentTime+5s);
        std::this_thread::sleep_for(7s); 

        d.removeBattery("bat2");

        d.destroyDirectory();
    }

    return 0;
}
