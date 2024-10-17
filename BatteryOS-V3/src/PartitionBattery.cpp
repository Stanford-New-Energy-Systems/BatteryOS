#include "PartitionBattery.hpp"

// add destructor
// quit() function?
PartitionBattery::~PartitionBattery() {
    PRINT() << "PARTITION BATTERY DESTRUCTOR" << std::endl;
    if (!this->quitThread)
        quit();
    if (this->runningThread) {
        this->status.current_mA = 0;
        this->thread.join();
    }
}

PartitionBattery::PartitionBattery(const std::string &batteryName,    
                                   const std::chrono::milliseconds &maxStaleness,
                                   const RefreshMode &refreshMode) : VirtualBattery(batteryName,
                                                                                    maxStaleness,
                                                                                    refreshMode)
{
    this->runningThread = false;
    this->requested_current_mA = 0;
    this->type = BatteryType::Partition;
    this->eventThread = std::thread(&PartitionBattery::runEventThread, this);
}

/*****************
Private Functions
******************/

void PartitionBattery::runChargingThread() {
    double capacity = this->status.capacity_mAh;

    while (this->status.current_mA != 0) {
        if (this->status.current_mA < 0) {
            if (-this->status.current_mA > this->status.max_charging_current_mA)
                this->status.current_mA = -this->status.max_charging_current_mA;
            else if (this->status.current_mA != this->requested_current_mA)
                this->status.current_mA = this->requested_current_mA; 
        } else {
            if (this->status.current_mA > this->status.max_discharging_current_mA)
                this->status.current_mA = this->status.max_discharging_current_mA;
            else if (this->status.current_mA != this->requested_current_mA)
                this->status.current_mA = this->requested_current_mA;
        }

        capacity += (-this->status.current_mA / 20);

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
//        std::this_thread::sleep_for(std::chrono::minutes(3));
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    return;
}

/*******************
Protected Functions
********************/

BatteryStatus PartitionBattery::refresh() {
    this->status.time = convertToMilliseconds(getTimeNow());
    return this->status;
}

bool PartitionBattery::set_current(double current_mA) {
    this->status.current_mA = current_mA;
    this->requested_current_mA = current_mA;

    if (current_mA == 0) {
        if (this->runningThread) {
            this->thread.join();
            this->runningThread = false; 
        }
    } else if (!this->runningThread) {
        this->runningThread = true;
        this->thread = std::thread(&PartitionBattery::runChargingThread, this);
    }

    return true; 
}


/****************
Public Functions
*****************/

std::string PartitionBattery::getSourceName() const {
    std::shared_ptr<PartitionManager> source = this->source.lock(); // weak_ptr to shared_ptr 
 
    return source->getBatteryName();
}

std::string PartitionBattery::getBatteryString() const {
    return "PartitionBattery";
}

void PartitionBattery::setSourceBattery(std::shared_ptr<PartitionManager> source) {
    this->source = source;

    this->status = source->initBatteryStatus(this->batteryName); // write this function
    
    if (this->refreshMode == RefreshMode::ACTIVE) {
        this->eventSet.insert(event_t(this->batteryName,
                                      EventID::REFRESH,
                                      0,
                                      getTimeNow() + this->getMaxStaleness(),
                                      getSequenceNumber()));
    }
    return;
}

bool PartitionBattery::schedule_set_current(double current_mA, timepoint_t startTime, timepoint_t endTime, std::string name, uint64_t sequenceNumber) {
    timepoint_t currentTime = getTimeNow();

    if (checkIfZero(this->status))
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    if (name == this->batteryName) {
        if (startTime < currentTime) {
            WARNING() << "start time of event has already passed!" << std::endl;
            return false; 
        } else if ((current_mA * -1) > getMaxChargingCurrent()) {
            WARNING() << "requested charging current: " << current_mA << "mA exceeds the maximum charging current: " << getMaxChargingCurrent() << "mA!" << std::endl;
            return false;
        } else if (current_mA > getMaxDischargingCurrent()) {
            WARNING() << "requested discharging current: " << current_mA << "mA  exceeds the maximum discharging current: " << getMaxDischargingCurrent() << "mA!" << std::endl;
            return false;
        }
    }

    std::shared_ptr<PartitionManager> bat = this->source.lock(); // weak_ptr to shared_ptr

    if (bat->schedule_set_current(current_mA, startTime, endTime, name, sequenceNumber)) {
        checkMergeAndInsertEvents(name, current_mA, startTime, endTime, sequenceNumber);
        this->condition_variable.notify_one();
    } else {
        WARNING() << "schedule_set_current command failed for one of the parent batteries ... command unsuccessful" << std::endl;
        return false;
    }

    return true;
}
