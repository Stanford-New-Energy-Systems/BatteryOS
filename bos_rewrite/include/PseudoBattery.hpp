#ifndef PSEUDO_BATTERY_HPP
#define PSEUDO_BATTERY_HPP

#include "PhysicalBattery.hpp"

class PseudoBattery: public PhysicalBattery {
    private:
        bool signal;
        lock_t cLock;
        bool runningThread;
        std::thread thread;
        std::condition_variable chargeNotifier; 

    public:
        virtual ~PseudoBattery();
        PseudoBattery(const std::string &batteryName,
                      const std::chrono::milliseconds &maxStaleness = std::chrono::milliseconds(1000),
                      const RefreshMode& refreshMode = RefreshMode::LAZY);

    private:
        void runChargingThread();

    protected:
        BatteryStatus refresh() override;
        bool set_current(double current_mA) override;

    public:
        double getCapacity();
        double getCapacityLock();
        double getMaxCapacity() const;
        void sendSignal();
};

#endif
