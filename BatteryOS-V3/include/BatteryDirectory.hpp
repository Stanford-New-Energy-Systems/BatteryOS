#ifndef BATTERY_DIRECTORY_HPP
#define BATTERY_DIRECTORY_HPP
#include <map>
#include <list>
#include <memory>
#include "BatteryInterface.hpp"

/**
* Battery Directory
* @param batteryMap:  map holding battery name and corrsponding pointer to the battery
* @param childGraph:  graph of child batteries pointing to parent batteries (used for battery removal) 
* @param parentGraph: graph of parent batteries pointing to child batteries (represents battery topology)
*/
class BatteryDirectory {
    private:
        bool destroyed;
        std::map<std::string, std::list<std::string>> childGraph;
        std::map<std::string, std::list<std::string>> parentGraph;
        std::map<std::string, std::shared_ptr<Battery>> batteryMap;
   
    /**
     * Constructors/Destructor
     * - Delete copy constructor and copy assignment
     * - Delete move constructor and move assignment
     */ 
    public:
        ~BatteryDirectory();
        BatteryDirectory();
        BatteryDirectory(BatteryDirectory&&) = delete;
        BatteryDirectory(const BatteryDirectory&) = delete;
        BatteryDirectory& operator=(BatteryDirectory&&) = delete;
        BatteryDirectory& operator=(const BatteryDirectory&) = delete;
    
    /**
     * Public Functions
     * @func addEdge:          adds an edge between batteries in the battery graphs
     * @func addBattery:       adds a battery to the directory
     * @func getBattery:       returns a battery from the directory
     * @func nameExists:       checks to see if battery name is in directory 
     * @func canBeSource:      checks if a battery can be a source for an aggregate or partition
     * @func removeBattery:    removes a battery from the directory
     * @func destroyDirectory: calls quit() on all the batteries in the topology (stops their eventThread)
     */
    public:
        void destroyDirectory();
        bool nameExists(const std::string &name);
        bool canBeSource(const std::string &batteryName);
        bool addBattery(std::shared_ptr<Battery> battery);
        bool removeBattery(const std::string &batteryName);
        bool addEdge(const std::string &parentName, const std::string &childName);
        std::shared_ptr<Battery> getBattery(const std::string &batteryName) const;
};


#endif
