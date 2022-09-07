#include "AggregateBattery.hpp"

AggregateBattery::~AggregateBattery() {
    PRINT() << "AGGREGATE DESTRUCTOR" << std::endl;
    if (!quitThread)
        quit();
}

AggregateBattery::AggregateBattery(const std::string &batteryName, 
                                   std::vector<std::shared_ptr<Battery>> parentBatteries,
                                   const std::chrono::milliseconds &maxStaleness,
                                   const RefreshMode &refreshMode) : VirtualBattery(batteryName,
                                                                                    maxStaleness,
                                                                                    refreshMode)
{
    this->type = BatteryType::Aggregate;
    this->parents = parentBatteries;      

    this->eventThread = std::thread(&AggregateBattery::runEventThread, this);
    
    this->eventSet.insert(event_t(this->batteryName,
                                  EventID::REFRESH,
                                  0,
                                  getTimeNow(),
                                  getSequenceNumber()));
    // at some point need to ensure parent battery currents
    // are at zero when first constructing the aggregate battery
}

double AggregateBattery::calc_c_rate(const double &current_mA, const double &capacity_mAh) {
    if (capacity_mAh == 0)
        return 0;
    return current_mA/capacity_mAh;
}

void AggregateBattery::set_c_rate() {
    std::vector<double> eff_charge_c_rates;
    std::vector<double> eff_discharge_c_rates;

    for (std::shared_ptr<Battery> battery : this->parents) {
        BatteryStatus pStatus = battery->getStatus();

        double charge_c_rate    = this->calc_c_rate(pStatus.max_charging_current_mA, (pStatus.max_capacity_mAh - pStatus.capacity_mAh));
        double discharge_c_rate = this->calc_c_rate(pStatus.max_discharging_current_mA, pStatus.capacity_mAh);

        eff_charge_c_rates.push_back(charge_c_rate);
        eff_discharge_c_rates.push_back(discharge_c_rate);
    }

    this->eff_charge_c_rate    = *std::min_element(eff_charge_c_rates.begin(), eff_charge_c_rates.end());
    this->eff_discharge_c_rate = *std::min_element(eff_discharge_c_rates.begin(), eff_discharge_c_rates.end());

    return;
}

BatteryStatus AggregateBattery::calcStatusVals() {
    this->set_c_rate();
   
    BatteryStatus newStatus{};    
    double max_effective_charge_power    = 0; 
    double max_effective_discharge_power = 0; 

    for (std::shared_ptr<Battery> battery : this->parents) {
        BatteryStatus pStatus = battery->getStatus();

        newStatus.current_mA += pStatus.current_mA;
        newStatus.capacity_mAh += pStatus.capacity_mAh;
        newStatus.max_capacity_mAh += pStatus.max_capacity_mAh;

        double chargeCapacity    = pStatus.max_capacity_mAh - pStatus.capacity_mAh;
        double dischargeCapacity = pStatus.capacity_mAh; 
        
        newStatus.max_charging_current_mA    += (double)(chargeCapacity * this->eff_charge_c_rate);
        newStatus.max_discharging_current_mA += (double)(dischargeCapacity  * this->eff_discharge_c_rate);

        max_effective_charge_power    += (chargeCapacity * this->eff_charge_c_rate) * pStatus.voltage_mV;
        max_effective_discharge_power += (dischargeCapacity * this->eff_discharge_c_rate) * pStatus.voltage_mV;
    }
    
    if (newStatus.max_charging_current_mA != 0)
        newStatus.voltage_mV = (double)(max_effective_charge_power / newStatus.max_charging_current_mA); // report charge voltage always 
    else
        newStatus.voltage_mV = (double)(max_effective_discharge_power / newStatus.max_discharging_current_mA); // report charge voltage always 

    newStatus.time = convertToMilliseconds(getTimeNow());

    return newStatus;
}

// should you move BatteryStatus instead of copy? 
// maybe do later  if you want to increase performance
// on a refresh show the discharge battery voltage
BatteryStatus AggregateBattery::refresh() {
    PRINT() << "Aggregate Battery Refresh!!!" << std::endl;
    return this->calcStatusVals();
}

bool AggregateBattery::schedule_set_current(double current_mA, timepoint_t startTime, timepoint_t endTime, std::string name, uint64_t sequenceNumber) {
    timepoint_t currentTime = getTimeNow(); 

    if (checkIfZero(this->status))
        std::this_thread::sleep_for(std::chrono::milliseconds(1000)); 

    this->lock.lock();
    this->status = this->checkAndRefresh();
    this->lock.unlock();
    
    if (name == this->batteryName) {
        if (startTime < currentTime) {
            WARNING() << "start time of event has already passed!" << std::endl;
            return false; 
        } else if ((current_mA * -1) > getMaxChargingCurrent()) {
            WARNING() << "requested charging current exceeds the maximum charging current!" << std::endl;
            return false;
        } else if (current_mA > getMaxDischargingCurrent()) {
            WARNING() << "requested discharging current exceeds the maximum discharging current!" << std::endl;
            return false;
        }
    }

    double c_rate = this->calc_c_rate(current_mA, this->status.capacity_mAh);

    for (std::shared_ptr<Battery> battery : this->parents) {
        double target_current_mA;
    
        BatteryStatus pStatus    = battery->getStatus();
        double chargeCapacity    = pStatus.max_capacity_mAh - pStatus.capacity_mAh;
        double dischargeCapacity = pStatus.capacity_mAh; 

        if (current_mA < 0) 
            target_current_mA = (double)(-chargeCapacity * c_rate);
        else
            target_current_mA = (double)(dischargeCapacity * c_rate);
        
        if (!battery->schedule_set_current(target_current_mA, startTime, endTime, name, sequenceNumber)) {
            WARNING() << "schedule_set_current command failed for one of the parent batteries ... command unsuccessful" << std::endl;
            return false;
        }
    } 

    return true;
}

std::string AggregateBattery::getBatteryString() const {
    return "AggregateBattery";
}
