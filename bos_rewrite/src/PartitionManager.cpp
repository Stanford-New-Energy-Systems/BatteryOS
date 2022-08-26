#include "PartitionManager.hpp"


// add destructor
// quit eventThread??
PartitionManager::~PartitionManager() {
    PRINT() << "PARTITION MANAGER DESTRUCTOR" << std::endl;
    if (!this->quitThread)
        quit();
}

PartitionManager::PartitionManager(const std::string &batteryName,
                                   std::vector<Scale> proportions,
                                   const PolicyType &partitionPolicyType,
                                   std::shared_ptr<Battery> sourceBattery,
                                   std::vector<std::weak_ptr<VirtualBattery>> childBatteries) : VirtualBattery(batteryName)
{
    this->refreshMode  = RefreshMode::ACTIVE; 
    this->maxStaleness = std::chrono::minutes(1);

    this->source     = sourceBattery;
    this->policyType = partitionPolicyType;

    this->children = childBatteries;
    this->child_proportions = proportions;
    
    this->type     = BatteryType::PartitionManager;
    this->status   = this->source->getStatus(); // parent current should be at 0 

    this->eventSet.insert(event_t(this->batteryName,
                                  EventID::REFRESH,
                                  0,
                                  getTimeNow() + this->maxStaleness,
                                  getSequenceNumber())); 
    this->eventThread = std::thread(&PartitionManager::runEventThread, this);
}

/*****************
Private Functions 
******************/

void PartitionManager::refreshTranched(const BatteryStatus &pStatus) {
    return;
}

void PartitionManager::refreshReserved(const BatteryStatus &pStatus) {
    return;
}

void PartitionManager::refreshProportional(const BatteryStatus &pStatus) {
    for (int index = 0; index < this->children.size(); index++) {
        std::shared_ptr<VirtualBattery> bat = this->children[index].lock();  // weak_ptr to shared_ptr

        bat->setMaxChargingCurrent(pStatus.max_charging_current_mA * child_proportions[index].charge_proportion);
        bat->setMaxDischargingCurrent(pStatus.max_discharging_current_mA * child_proportions[index].charge_proportion);
    }
    return;
}

/*******************
Protected Functions 
********************/

BatteryStatus PartitionManager::refresh() {
    PRINT() << "Partition Manager Refresh!!!!" << std::endl;
    BatteryStatus pStatus = this->source->getStatus();
    // check pStatus w status to ensure that status values are the same
    // if status values change, iteract with child structs to edit the information
    if (this->policyType == PolicyType::PROPORTIONAL)
        this->refreshProportional(pStatus);
    else if (this->policyType == PolicyType::RESERVED)
        this->refreshReserved(pStatus);
    else
        this->refreshTranched(pStatus);

    return this->status;
}

bool PartitionManager::set_current(double current_mA) {
    this->status.current_mA = current_mA;
    return true;
}

bool PartitionManager::schedule_set_current(double current_mA, timepoint_t startTime, timepoint_t endTime, std::string name, uint64_t sequenceNumber) {
    if (this->source->schedule_set_current(current_mA, startTime, endTime, name, sequenceNumber)) {
        checkMergeAndInsertEvents(name, current_mA, startTime, endTime, sequenceNumber);
        this->condition_variable.notify_one(); 
    } else {
        WARNING() << "schedule_set_current command failed for one of the parent batteries ... command unsuccessful" << std::endl;
        return false;
    }

    return true;
}

BatteryStatus PartitionManager::initBatteryStatus(const std::string &childName) {
    int index;
    BatteryStatus childStatus;

    for (index = 0; index < this->children.size(); index++) {
        std::shared_ptr<VirtualBattery> bat = this->children[index].lock();
        
        if (bat->getBatteryName() == childName)
            break;
    }

    double capacity      = this->status.capacity_mAh;
    double max_charge    = this->status.max_charging_current_mA;
    double max_capacity  = this->status.max_capacity_mAh;
    double max_discharge = this->status.max_discharging_current_mA;

    double charge_proportion   = child_proportions[index].charge_proportion;
    double capacity_proportion = child_proportions[index].capacity_proportion;

    childStatus.voltage_mV                 = this->status.voltage_mV;
    childStatus.current_mA                 = this->status.current_mA; // should be 0 on init 
    childStatus.capacity_mAh               = capacity * capacity_proportion; 
    childStatus.max_capacity_mAh           = max_capacity * capacity_proportion; 
    childStatus.max_charging_current_mA    = max_charge * charge_proportion; 
    childStatus.max_discharging_current_mA = max_discharge * charge_proportion;

    return childStatus;
}


std::string PartitionManager::getBatteryString() const {
    return "PartitionManager";
}
