#include "BatteryDirectoryManager.hpp"

/**********************
Constructor/Destructor
***********************/

BatteryDirectoryManager::~BatteryDirectoryManager() {};

BatteryDirectoryManager::BatteryDirectoryManager() {
    this->pManagerCounter = 1;
    this->directory = std::make_unique<BatteryDirectory>();
}

/*****************
Private Functions
******************/

std::string BatteryDirectoryManager::createPartitionManagerName() {
    return "pManager" + std::to_string(this->pManagerCounter++);
}

/****************
Public Functions 
*****************/

void BatteryDirectoryManager::destroyDirectory() {
    return this->directory->destroyDirectory();
}

std::shared_ptr<Battery> BatteryDirectoryManager::getBattery(const std::string &name) const {
    return this->directory->getBattery(name);
}

bool BatteryDirectoryManager::removeBattery(const std::string &name) {
    std::shared_ptr<Battery> battery = this->directory->getBattery(name);

    if (battery == nullptr)
        return false;

    if (battery->getBatteryType() == BatteryType::Partition) {
        std::string source = std::static_pointer_cast<PartitionBattery>(battery)->getSourceName();
        return this->directory->removeBattery(source);
    }
    
    return this->directory->removeBattery(name);
}

std::shared_ptr<Battery> BatteryDirectoryManager::createPhysicalBattery(const std::string &name,
                                                                        const std::chrono::milliseconds &maxStaleness, 
                                                                        const RefreshMode &refreshMode)
{
    std::shared_ptr<Battery> battery = std::make_shared<PhysicalBattery>(name, maxStaleness, refreshMode);

    if (!this->directory->addBattery(battery))
        return nullptr; 
    return battery;
}

std::shared_ptr<Battery> BatteryDirectoryManager::createDynamicBattery(void* initArgs,
                                                                       void* destructor,
                                                                       void* constructor,
                                                                       void* refreshFunc,
                                                                       void* setCurrentFunc, 
                                                                       const std::string& batteryName, 
                                                                       const std::chrono::milliseconds& maxStaleness,
                                                                       const RefreshMode& refreshMode)
{
    std::shared_ptr<Battery> battery = std::make_shared<DynamicBattery>(initArgs, destructor, constructor,
                                                                        refreshFunc, setCurrentFunc, batteryName,
                                                                        maxStaleness, refreshMode);

    if (!this->directory->addBattery(battery))
        return nullptr;
    return battery;  
}

std::shared_ptr<Battery> BatteryDirectoryManager::createAggregateBattery(const std::string &name,
                                                                          std::vector<std::string> parentNames,
                                                                          const std::chrono::milliseconds &maxStaleness, 
                                                                          const RefreshMode &refreshMode)
{
    std::vector<std::shared_ptr<Battery>> parents;

    for (int i = 0; i < parentNames.size(); i++) {
        std::shared_ptr<Battery> battery = this->directory->getBattery(parentNames[i]);

        if (battery == nullptr)
            return nullptr;
        else if (battery->getCurrent() != 0) {
            WARNING() << "parent batteries must not be charging/discharging to create aggregate battery ... turn batteries off" << std::endl;
            return nullptr;
        } else if (!this->directory->canBeSource(parentNames[i])) {
            WARNING() << parentNames[i] << " is already a source battery ... delete child batteries first before using again as a source" << std::endl;
            return nullptr;
        }
        
        parents.push_back(battery);            
    } 

    if (parents.size() == 0) {
        WARNING() << "must provide parent names to create aggregate battery" << std::endl;
        return nullptr;
    }

    std::shared_ptr<Battery> battery = std::make_shared<AggregateBattery>(name, parents, maxStaleness, refreshMode);

    if (!this->directory->addBattery(battery))
        return nullptr;

    for (int i = 0; i < parentNames.size(); i++) {
        this->directory->addEdge(parentNames[i], name);
    }    

    return battery;
}

std::vector<std::shared_ptr<Battery>> BatteryDirectoryManager::createPartitionBattery(const std::string &sourceName,
                                                                         const PolicyType &policyType, 
                                                                         std::vector<std::string> names, 
                                                                         std::vector<Scale> child_proportions,
                                                                         std::vector<std::chrono::milliseconds> maxStalenesses,
                                                                         std::vector<RefreshMode> refreshModes)
{
    Scale s;
    for (int i = 0; i < child_proportions.size(); i++) {
        s += child_proportions[i];
    }

    if (s.charge_proportion != 1 || s.capacity_proportion != 1) {
        WARNING() << "sum of charge and capacity proportions must equal 1" << std::endl;
        return std::vector<std::shared_ptr<Battery>>();
    } else if (!allequal(names.size(), child_proportions.size(), maxStalenesses.size(), refreshModes.size())) {
        WARNING() << "size of vectors are not the same" << std::endl;
        return std::vector<std::shared_ptr<Battery>>();
    } else if (!this->directory->canBeSource(sourceName)) {
        WARNING() << "this battery is already a source battery ... delete child batteries first before using again as a source" << std::endl;
        return std::vector<std::shared_ptr<Battery>>();
    }

    std::vector<std::shared_ptr<Battery>> batteries;
    std::vector<std::weak_ptr<VirtualBattery>> children;    
    std::vector<std::shared_ptr<PartitionBattery>> partitions;

    for (int i = 0; i < names.size(); i++) {
        std::shared_ptr<PartitionBattery> battery = std::make_shared<PartitionBattery>(names[i], maxStalenesses[i], refreshModes[i]);    

        children.push_back(battery);
        batteries.push_back(battery);
        partitions.push_back(battery);
    }

    std::shared_ptr<Battery> source = this->directory->getBattery(sourceName);

    if (source == nullptr) 
        return std::vector<std::shared_ptr<Battery>>();
    else if (source->getCurrent() != 0) {
        WARNING() << "source battery must not be charging/discharging to create partition battery ... turn battery off" << std::endl;
        return std::vector<std::shared_ptr<Battery>>();
    } 

    std::string pManager = this->createPartitionManagerName();
    std::shared_ptr<PartitionManager> manager = std::make_shared<PartitionManager>(pManager, child_proportions, policyType, source, children);

    for (int i = 0; i < partitions.size(); i++) {
        partitions[i]->setSourceBattery(manager);
    }

    if (!this->directory->addBattery(manager)) { // should never hit this ERROR 
        ERROR() << "could not add partition manager: " << pManager << " to directory";
        return std::vector<std::shared_ptr<Battery>>();
    }

    this->directory->addEdge(sourceName, pManager);

    for (int i = 0; i < names.size(); i++) {
        if (!this->directory->addBattery(partitions[i]))
            return std::vector<std::shared_ptr<Battery>>();

        this->directory->addEdge(pManager, names[i]);
    } 
    
    return batteries;
}

std::vector<std::shared_ptr<Battery>> BatteryDirectoryManager::createPartitionBattery(const std::string &sourceName,
                                                                                      const PolicyType &policyType, 
                                                                                      std::vector<std::string> names, 
                                                                                      std::vector<Scale> child_proportions)
{
    std::vector<RefreshMode> refreshModes;
    std::vector<std::chrono::milliseconds> maxStalenesses;

    for (int i = 0; i < names.size(); i++) {
        refreshModes.push_back(RefreshMode::LAZY);
        maxStalenesses.push_back(std::chrono::milliseconds(1000));
    }

    return this->createPartitionBattery(sourceName, policyType, names, child_proportions, maxStalenesses, refreshModes);
}
