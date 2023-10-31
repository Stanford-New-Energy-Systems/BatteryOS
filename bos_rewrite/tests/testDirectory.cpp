#include "PhysicalBattery.hpp"
#include "BatteryDirectory.hpp"

#include <iostream>

int main() {
    using namespace std::chrono_literals;
    BatteryDirectory d;
    {
        std::shared_ptr<Battery> bat0 = std::make_shared<PhysicalBattery>("bat0", 1200);
        std::shared_ptr<Battery> bat1 = std::make_shared<PhysicalBattery>("bat1", 1200);
        std::shared_ptr<Battery> bat2 = std::make_shared<PhysicalBattery>("bat2", 1200);
        d.addBattery(bat0);
        d.addBattery(bat1);
        d.addBattery(bat2);
    }
    d.addEdge("bat0", "bat1");
    d.addEdge("bat1", "bat2");
//    d.removeBattery("bat2");
//    d.removeBattery("bat1");
    d.removeBattery("bat0");
    //if (!d.addEdge("bat1", "bat2"))
    //    std::cout << "FALSE" << std::endl;
    std::this_thread::sleep_for(5s); 
    std::cout << "Hello World!" << std::endl;
    return 0;
}
