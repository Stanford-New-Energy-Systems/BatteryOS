#include "BatteryDirectory.hpp"
#include <iostream>

/**********************
Constructor/Destructor
**********************/

BatteryDirectory::~BatteryDirectory() {
    if (!this->destroyed)
        destroyDirectory();
}

BatteryDirectory::BatteryDirectory() {
    this->destroyed = false;
}

/****************
Public Functions
*****************/

bool BatteryDirectory::nameExists(const std::string &name) {
    return batteryMap.count(name) == 1;
}

bool BatteryDirectory::addBattery(std::shared_ptr<Battery> battery) {
    std::string name = battery->getBatteryName();
    if (batteryMap.count(name) == 1) {
        WARNING() << name << " already exists in directory! choose a unique battery name" << std::endl;
        return false;
    } 
    
    batteryMap.insert({name, battery}); 
    return true;
}

// used by BatteryDirectoryManager
// check if battery can be source
// before adding edges in directory
bool BatteryDirectory::canBeSource(const std::string &batteryName) {
    if (batteryMap.count(batteryName) != 1) {
        WARNING() << batteryName << " does not exist in directory!" << std::endl;
        return false;
    } else if (parentGraph.count(batteryName) == 1) {
        return false;
    }
    return true; 
}

bool BatteryDirectory::addEdge(const std::string &parentName, const std::string &childName) {
    if (batteryMap.count(parentName) != 1) {
        WARNING() << "parent name: " << parentName << " does not exist in directory!" << std::endl;
        return false;
    } else if (batteryMap.count(childName) != 1) {
        WARNING() << "child name: " << childName << " does not exist in directory!" << std::endl;
        return false;
    } 
    
    childGraph[childName].push_back(parentName);
    parentGraph[parentName].push_back(childName);
    return true; 
}

std::shared_ptr<Battery> BatteryDirectory::getBattery(const std::string &batteryName) const {
    if (batteryMap.count(batteryName) != 1) {
        WARNING() << batteryName << " does not exist in directory!" << std::endl;
        return nullptr;
    }

    return batteryMap.at(batteryName);
}

// when removing a child of a partition, you should 
// remove all the children of the partition as well 
// as the partition manager ... think of a clever way
// to handle this 

// or BatteryDirectoryManager is responsible for this
// whenever the BatteryDirectoryManager gets a remove command
// it checks to see if the battery is a partition and if it is 
// it sends the removeBattery command with the name of the partition
// manager that handles the children 
bool BatteryDirectory::removeBattery(const std::string &batteryName) {
    if (batteryMap.count(batteryName) == 0) {
        WARNING() << batteryName << " does not exist in directory!" << std::endl;
        return false; 
    } else if (parentGraph.count(batteryName) == 1) {
        std::list<std::string> childNames = parentGraph[batteryName];
        for (std::string &child : childNames) {
            if (!this->removeBattery(child))
                return false;
        } 
        parentGraph.erase(batteryName);
    }
    if (childGraph.count(batteryName) == 1) {
        std::list<std::string> parentNames = childGraph[batteryName];
        for (std::string &parent : parentNames) {
            parentGraph[parent].remove(batteryName);
            if (parentGraph[parent].empty())
                parentGraph.erase(parent);
        }
        childGraph.erase(batteryName);
    }

    LOG() << "removing " << batteryName <<  " from directory" << std::endl;

    batteryMap.erase(batteryName);
    return true;
}

void BatteryDirectory::destroyDirectory() {
    if (!this->destroyed) {
        for (const auto &batteryIter : batteryMap) {
            batteryIter.second -> quit();
        }
    }
    this->batteryMap.clear();
    this->destroyed = true; 
}
