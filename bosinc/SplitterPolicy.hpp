#ifndef SPLITTER_POLICY_HPP
#define SPLITTER_POLICY_HPP

#include "BatteryInterface.hpp"
#include "BOSDirectory.hpp"
enum class SplitterPolicyType : int {
    Proportional, 
    Tranche, 
    Reservation,
};

class SplitterPolicy : public VirtualBattery {
public:
    struct child_current_event_t {
        timepoint_t timepoint;
        std::string childname;
        int64_t current_mA;
        bool is_greater_than_target;
    };
protected: 
    std::string src_name;
    BOSDirectory *pdirectory;
    Battery *source;
    SplitterPolicyType policy_type;
    
    std::map<std::string, int64_t> current_map;
    std::list<child_current_event_t> children_current_events;
public: 
    SplitterPolicy(
        const std::string &policy_name, 
        const std::string &src_name, 
        BOSDirectory &directory,
        SplitterPolicyType policy_type,
        std::chrono::milliseconds max_staleness
    ) : 
        VirtualBattery(policy_name, max_staleness), 
        src_name(src_name), 
        pdirectory(&directory), 
        policy_type(policy_type)
    {
        this->type = BatteryType::SplitterPolicy;
        source = directory.get_battery(src_name);
        if (!source) {
            WARNING() << ("source not found!");
        }
    }
    
    BatteryStatus refresh() override {
        ERROR() << "This function shouldn't be called";
        return this->status;
    }
    uint32_t set_current(int64_t current_mA, bool is_greater_than) override {
        ERROR() << "This function shouldn't be called";
        return 0;
    }
    
    /**
     * Get its source battery 
     * @return Battery* pointing to its source battery 
     */
    virtual Battery *get_source() {
        return this->source;
    }

    /**
     * Get its children
     * @return a list of Battery*
     */
    virtual std::list<Battery*> get_children() {
        lockguard_t lkg(this->lock);
        return this->pdirectory->get_children(this->name);
    }

    /**
     * Receive and compute the status of one of its children
     */
    virtual BatteryStatus get_status_of(const std::string &child_name) = 0;

    /**
     * Schedule a set_current event for one of its children
     */
    virtual uint32_t schedule_set_current_of(
        const std::string &child_name, 
        int64_t target_current_mA, 
        bool is_greater_than_target, 
        timepoint_t when_to_set, 
        timepoint_t until_when
    ) = 0;

    /**
     * fork battery child_name from child_name, 
     * and the status is target_status (might not be fulfilled)
     * @param from_name the name of the battery to fork from
     * @param to_name the child battery, notice that this must be created first 
     * @param target_status the target status of the child battery 
     * @return the actual status of the child battery 
     */
    virtual BatteryStatus fork_from(
        const std::string &from_name, 
        const std::string &child_name, 
        BatteryStatus target_status
    ) = 0;

    /**
     * Merge a battery to another battery
     * @param name the name of the battery to merge 
     * @param to_name the name of the battery to receive the portion 
     * @return the status of the battery to_name after the merge 
     */
    virtual void merge_to(
        const std::string &name, 
        const std::string &to_name
    ) = 0;


};



#endif // ! SPLITTER_POLICY_HPP
