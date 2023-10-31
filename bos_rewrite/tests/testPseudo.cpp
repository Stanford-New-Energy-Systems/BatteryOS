#include "PseudoBattery.hpp"
#include "PhysicalBattery.hpp"

int main() {
    using namespace std::chrono_literals;

    std::shared_ptr<Battery> bat0 = std::make_shared<PseudoBattery>("bat0", std::chrono::seconds(100));

    BatteryStatus status;

    status.voltage_mV = 5;
    status.current_mA = 0;
    status.capacity_mAh = 7500;
    status.max_capacity_mAh = 7500;
    status.max_charging_current_mA = 3600;
    status.max_discharging_current_mA = 3600;
    status.time = convertToMilliseconds(getTimeNow());

    bat0->setBatteryStatus(status);

    std::this_thread::sleep_for(1s);

    timepoint_t currentTime = getTimeNow();

    bat0->schedule_set_current(3600, currentTime+1s, currentTime+60s);

    std::this_thread::sleep_for(6s);
    PRINT() << bat0->getStatus() << std::endl;

    std::this_thread::sleep_for(6s);
    PRINT() << bat0->getStatus() << std::endl;

    std::this_thread::sleep_for(6s);
    PRINT() << bat0->getStatus() << std::endl;

    std::this_thread::sleep_for(6s);
    PRINT() << bat0->getStatus() << std::endl;

    std::this_thread::sleep_for(6s);
    PRINT() << bat0->getStatus() << std::endl;

    std::this_thread::sleep_for(6s);
    PRINT() << bat0->getStatus() << std::endl;

    std::this_thread::sleep_for(6s);
    PRINT() << bat0->getStatus() << std::endl;

    std::this_thread::sleep_for(6s);
    PRINT() << bat0->getStatus() << std::endl;

    std::this_thread::sleep_for(6s);
    PRINT() << bat0->getStatus() << std::endl;

    std::this_thread::sleep_for(6s);
    PRINT() << bat0->getStatus() << std::endl;

    return 0;
}
