#include "BatteryInterface.hpp"

uint64_t SEQUENCE_NUMBER = 1;
uint64_t getSequenceNumber(void) {
    return SEQUENCE_NUMBER++;
}
timepoint_t getTimeNow(void) {
    return std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()); 
}

Battery::~Battery() {};

Battery::Battery(const std::string &batteryName,
                 const std::chrono::milliseconds &maxStaleness, 
                 const RefreshMode &refreshMode) : batteryName(batteryName) 
{
    this->current_mA            = 0;
    this->quitThread            = false;
    this->refreshMode           = refreshMode;
    this->maxStaleness          = maxStaleness;
}


/*****************
BAL API Functions
******************/

BatteryStatus Battery::getStatus() {
    lockguard_t mutexLock(this->lock);
    if (this->refreshMode == RefreshMode::LAZY)
        this->status = this->checkAndRefresh();
    return this->status;
}

bool Battery::schedule_set_current(double current_mA, timepoint_t startTime, timepoint_t endTime, std::string name, uint64_t sequenceNumber) {
    timepoint_t currentTime = getTimeNow(); 

    if (this->status.checkIfZero())
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

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

    checkMergeAndInsertEvents(name, current_mA, startTime, endTime, sequenceNumber);
    this->condition_variable.notify_one();
    return true;
    // use delay to decrease startTime and endTime to work with battery
}

bool Battery::schedule_set_current(double current_mA, timepoint_t startTime, timepoint_t endTime) {
    return schedule_set_current(current_mA, startTime, endTime, this->batteryName, getSequenceNumber());
}

bool Battery::schedule_set_current(double current_mA, uint64_t startTime, uint64_t endTime) {
    timepoint_t start;
    timepoint_t end;

    start += std::chrono::milliseconds(startTime);
    end   += std::chrono::milliseconds(endTime);

    return schedule_set_current(current_mA, start, end);
}


/**************************
Protected Helper Functions
***************************/

void Battery::runEventThread() {
    std::unique_lock<lock_t> uniqueLock(this->lock);
    while (!(this->quitThread)) {
        if (this->eventSet.size() == 0)
            this->condition_variable.wait(uniqueLock, [this]{ return this->eventSet.size() > 0 || this->quitThread; }); 
        if (this->quitThread)
            return;
        
        while ( !(getTimeNow() >= this->eventSet.begin()->eventTime || this->quitThread)) {
            if (this->condition_variable.wait_until(uniqueLock, this->eventSet.begin()->eventTime) == std::cv_status::timeout)
                break;
        }

        if (this->quitThread)
            return;

        EventVector eventVector;
        EventSet::iterator iter;
        timepoint_t currentTime = getTimeNow();

        for (iter = eventSet.begin(); iter != eventSet.end(); iter++) {
            if (iter->eventTime > currentTime)
                break;
            else if (iter->eventID == EventID::SET_CURRENT_BEGIN)
                eventVector.push_back(*iter);
            else if (iter->eventID == EventID::SET_CURRENT_END)
                eventVector.push_back(*iter);
            else if (iter->eventID == EventID::CANCEL_SET_CURRENT_EVENT)
                break;
            else {
                if (this->refreshMode == RefreshMode::ACTIVE) {
                    event_t refreshEvent = event_t(iter->batteryName,
                                                   iter->eventID,
                                                   iter->current_mA,
                                                   iter->eventTime + this->maxStaleness,
                                                   getSequenceNumber());
                    eventSet.insert(refreshEvent);
                } 
                this->status = refresh();
                eventVector.push_back(*iter);
            }
        }

        if (iter->eventID == EventID::CANCEL_SET_CURRENT_EVENT) {
            EventPair eventPair = eventMap.at(iter->sequenceNumber);
            eventSet.erase(eventPair.first);
            eventSet.erase(eventPair.second);
            eventSet.erase(*iter);
            eventMap.erase(iter->sequenceNumber);
        }
        
        if (!(eventSet.empty())) {
            double old_current_mA = this->current_mA;
            for (event_t &event : eventVector) {
                if (event.eventID == EventID::SET_CURRENT_BEGIN) 
                    this->current_mA += event.current_mA;
                else if (event.eventID == EventID::SET_CURRENT_END) {
                    this->current_mA -= event.current_mA;
                    eventMap.erase(event.sequenceNumber);
                }
                eventSet.erase(event);
            }
            if (this->current_mA != old_current_mA)
                set_current(this->current_mA); 
        }
    }
}

BatteryStatus Battery::checkAndRefresh() {
    if (this->refreshMode == RefreshMode::LAZY) {
        timepoint_t currentTime = getTimeNow();
        if (currentTime - this->status.timestamp.time > this->maxStaleness)
            return this->refresh();
    }
    return this->status;    
}

void Battery::checkMergeAndInsertEvents(std::string batteryName, double current_mA, timepoint_t startTime, timepoint_t endTime, uint64_t sequenceNumber) {
    double target_current_mA = current_mA;
 
    lockguard_t mutexLock(this->lock);

    if (eventMap.count(sequenceNumber) == 1) {
        EventPair pair = eventMap.at(sequenceNumber);
        eventSet.erase(pair.first);
        eventSet.erase(pair.second);
        eventMap.erase(sequenceNumber);
        target_current_mA += pair.first.current_mA;
    } else {
        for (const auto &iter : eventMap) {
            EventPair currPair = iter.second;
            event_t startEvent = currPair.first;
            event_t endEvent   = currPair.second;
            
            if (batteryName != startEvent.batteryName) {
                continue;
            } else if (startTime <= startEvent.eventTime && endTime >= endEvent.eventTime) {
                event_t cancelEvent = event_t(batteryName,
                                              EventID::CANCEL_SET_CURRENT_EVENT,
                                              0,
                                              startTime,
                                              startEvent.sequenceNumber);
                eventSet.insert(cancelEvent);
            } else if (startTime >= startEvent.eventTime && endTime < endEvent.eventTime) {
                target_current_mA -= startEvent.current_mA;
            } else if (startTime >= startEvent.eventTime && endTime >= endEvent.eventTime) {
                eventSet.erase(endEvent);
                event_t newEvent = event_t(batteryName,
                                           EventID::SET_CURRENT_END, 
                                           endEvent.current_mA,
                                           startTime,
                                           endEvent.sequenceNumber);
                eventSet.insert(newEvent);

                EventPair oldPair = eventMap.at(endEvent.sequenceNumber);
                oldPair.second = newEvent;
                eventMap.at(endEvent.sequenceNumber) = oldPair;
            } else if (startTime <= startEvent.eventTime && endTime <= endEvent.eventTime) {
                eventSet.erase(startEvent);
                event_t newEvent = event_t(batteryName,
                                           EventID::SET_CURRENT_BEGIN,
                                           startEvent.current_mA,
                                           endTime,
                                           startEvent.sequenceNumber);
                eventSet.insert(newEvent);

                EventPair oldPair = eventMap.at(startEvent.sequenceNumber);
                oldPair.first = newEvent;
                eventMap.at(startEvent.sequenceNumber) = oldPair;
            }
        }
    }

    event_t beginEvent = event_t(batteryName,
                                 EventID::SET_CURRENT_BEGIN,
                                 target_current_mA,
                                 startTime,
                                 sequenceNumber);
    event_t endEvent   = event_t(batteryName,
                                 EventID::SET_CURRENT_END,
                                 target_current_mA,
                                 endTime,
                                 sequenceNumber);

    eventSet.insert(beginEvent);
    eventSet.insert(endEvent);

    EventPair eventPair = std::make_pair(beginEvent, endEvent);
    eventMap.insert({sequenceNumber, eventPair});

    return;
}


/***********************
Public Helper Functions
************************/

void Battery::quit() {
    this->lock.lock();
    this->quitThread = true; 
    this->lock.unlock(); 

    this->condition_variable.notify_one();
    eventThread.join();
}

double Battery::getCurrent() const {
    return this->status.current_mA;
}

std::string Battery::getBatteryName() const {
    return this->batteryName;
}

double Battery::getMaxChargingCurrent() const {
    return this->status.max_charging_current_mA;
}


double Battery::getMaxDischargingCurrent() const {
    return this->status.max_discharging_current_mA;
}

std::string Battery::getBatteryString() const {
    return "BatteryInterface";
}

std::chrono::milliseconds Battery::getMaxStaleness() const {
    return this->maxStaleness;
}

void Battery::setRefreshMode(const RefreshMode &refreshMode) {
    if (this->refreshMode == refreshMode)
        return;

    lockguard_t mutexLock(this->lock);
    this->refreshMode = refreshMode;
    if (this->refreshMode == RefreshMode::ACTIVE) {
        this->status = refresh();
        timepoint_t currentTime = getTimeNow();
        event_t refreshEvent    = event_t(this->batteryName,
                                          EventID::REFRESH,
                                          0,
                                          currentTime + getMaxStaleness(),
                                          getSequenceNumber());
        eventSet.insert(refreshEvent); 
        this->condition_variable.notify_one();
    }
    return;
}

//bool Battery::cancelEvent(timepoint_t startTime, timepoint_t endTime) {
//    return schedule_set_current(0, startTime, endTime, this->batteryName);
//}

void Battery::setMaxStaleness(const std::chrono::milliseconds &maxStaleness) {
    lockguard_t mutexLock(this->lock);
    this->maxStaleness = maxStaleness;
}
