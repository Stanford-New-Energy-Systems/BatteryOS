#include "PseudoBattery.hpp"

PseudoBattery::~PseudoBattery() {
    PRINT() << "PSEUDO BATTERY DESTRUCTOR" << std::endl;

    if (!this->quitThread)
        quit();
    if (this->runningThread) {
        this->status.current_mA = 0;
        this->thread.join();
    }
}

PseudoBattery::PseudoBattery(const std::string& batteryName,
                             const std::chrono::milliseconds& maxStaleness,
                             const RefreshMode& refreshMode) : PhysicalBattery(batteryName,
                                                                               maxStaleness,
                                                                               refreshMode)
{
    this->signal = false;
    this->runningThread = false;
    this->type = BatteryType::Physical;
}

/****************
Private Functions
*****************/

void PseudoBattery::runChargingThread() {
    std::unique_lock<lock_t> uLock(this->cLock);
    double capacity = this->status.capacity_mAh;

    while (this->status.current_mA != 0) {
        this->signal = false;

        while(!this->signal)
            this->chargeNotifier.wait(uLock);

        capacity += (-this->status.current_mA); // dividing by 20 used for 3 minute intervals

        if (this->status.current_mA < 0) {
            if (capacity >= this->status.max_capacity_mAh) {
                capacity = this->status.max_capacity_mAh;
                this->status.current_mA = 0;
            }
        } else if (this->status.current_mA > 0) {
            if (capacity <= 0) {
                capacity = 0;
                this->status.current_mA = 0;
            }   
        }
        
        this->status.capacity_mAh = capacity;

//        while (!this->signal)  
//            this->chargeNotifier.wait(uLock); // these two lines down here for capacityLock function

//        std::this_thread::sleep_for(std::chrono::minutes(3));
//        std::this_thread::sleep_for(std::chrono::seconds(1));
//        std::this_thread::sleep_for(std::chrono::milliseconds(1005));
    }
    return;
}

/******************
Protected Functions
*******************/

BatteryStatus PseudoBattery::refresh() {
    this->status.time = convertToMilliseconds(getTimeNow());
    return this->status;
}

bool PseudoBattery::set_current(double current_mA) {
    this->status.current_mA = current_mA;
    
    if (current_mA == 0) {
        if (this->runningThread) {
            this->thread.join();
            this->runningThread = false;
        }
    } else if (!this->runningThread) {
        this->runningThread = true;
        this->thread = std::thread(&PseudoBattery::runChargingThread, this);
    }
    
    return true;
}

/***************
Public Functions
****************/

double PseudoBattery::getCapacityLock() {
    this->cLock.lock();
    this->signal = true;
    this->cLock.unlock();

    double capacity = this->status.capacity_mAh;
    this->chargeNotifier.notify_one();   
    return capacity;
}

double PseudoBattery::getCapacity() {
    lockguard_t gLock(this->cLock);
    return this->status.capacity_mAh;
}

double PseudoBattery::getMaxCapacity() const {
    return this->status.max_capacity_mAh;
}

/*******************
PID Helper Function
********************/

void PseudoBattery::sendSignal() {
    this->cLock.lock();
    this->signal = true;
    this->cLock.unlock();

    this->chargeNotifier.notify_all();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return; 
}
