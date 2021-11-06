#ifndef POLICY_HPP
#define POLICY_HPP

#include "BatteryInterface.hpp"
#include "BOSDirectory.hpp"
enum class SplitterPolicyType : int {
    Proportional, 
    Tranche, 
    Reservation,
};

/**
 * Policy must start background refresh after construction!!! 
 */
class SplitterPolicy : public VirtualBattery {
protected: 
    std::string src_name;
    BOSDirectory *pdirectory;
    Battery *source;
    SplitterPolicyType policy_type;
    /// the requested currents of each children 
    std::map<std::string, int64_t> children_current_now;
    /// the status of each children 
    std::map<std::string, BatteryStatus> children_status_now;
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
        if (!source) { ERROR() << ("source not found!"); }
    }
protected: 
    /* 
        Please override refresh in derived classes 
        because different policies will have different schemes for determining children status 
    */
    // BatteryStatus refresh() override {
    //     return this->status;
    // }

    /** 
     * set current is to update the theoretic currents of its children! 
     * the real set current is already forwarded to the source battery already 
     */
    uint32_t set_current(int64_t current_mA, bool is_greater_than, void *child_name_vptr) override {
        if (!child_name_vptr) return 0;
        std::string *child_name_ptr = (std::string*)child_name_vptr;
        if (!child_name_ptr) return 0;
        this->children_current_now[*child_name_ptr] = current_mA;
        // NOTE: please do delete the pointer! 
        delete child_name_ptr;
        return 1;
    }
public: 
    /// get_status is forbidden 
    BatteryStatus get_status() override { ERROR() << "This function shouldn't be called"; return this->status; }
    /// schedule_set_current is forbidden
    uint32_t schedule_set_current(int64_t, bool, timepoint_t, timepoint_t) override {
        ERROR() << "This function shouldn't be called"; return 0; }
    
    /**
     * Get its source battery 
     * @return Battery* pointing to its source battery 
     */
    virtual Battery *get_source() {
        lockguard_t lkg(this->lock);
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
    virtual BatteryStatus get_status_of(const std::string &child_name) {
        lockguard_t lkg(this->lock);
        switch (this->refresh_mode) {
        case RefreshMode::ACTIVE:
            return children_status_now[child_name];
        case RefreshMode::LAZY: 
            this->check_staleness_and_refresh();
            return children_status_now[child_name];
        default: 
            WARNING() << "unknown refresh mode"; 
            return children_status_now[child_name];
        }
        return children_status_now[child_name];
    }

    /**
     * Schedule a set_current event for one of its children
     */
    virtual uint32_t schedule_set_current_of(
        const std::string &child_name, 
        int64_t target_current_mA, 
        bool is_greater_than_target, 
        timepoint_t when_to_set, 
        timepoint_t until_when
    ) {
        {
            lockguard_t lkd(this->lock);
            timepoint_t now = get_system_time();
            if (when_to_set < now) {
                WARNING() << "when_to_set must be at least now";
                return 0;
            }
            if (target_current_mA > children_status_now[child_name].max_discharging_current_mA || 
                (-target_current_mA) > children_status_now[child_name].max_charging_current_mA) {
                WARNING() << "target current too high, event not scheduled";
                return 0;
            }
            // if there's overlapping set current events, merge the ranges! 
            // this might cause problems??? 
            // pop out the events that are earlier than when_to_set and save them

            // preview the current events from now to until_when 
            // and determine how much current to resume to 

            std::map<std::string, int64_t> current_up_to_now = this->children_current_now; // copy! 

            int64_t child_current_backup = 0;
            
            int64_t total_currents_to_set = 0;
            int64_t total_currents_to_resume = 0;

            std::vector<EventQueue::iterator> events_to_remove;
 
            EventQueue::iterator event_iterator;
            // preview the events up to when_to_set  
            for (
                event_iterator = event_queue.begin(); 
                event_iterator != event_queue.end(); 
                ++event_iterator
            ) {
                std::string *pchild_name = (std::string*)event_iterator->other_data;
                if (event_iterator->timepoint > when_to_set) {
                    // now we know about the current situations on when_to_set 
                    break;
                }
                if (event_iterator->func == Function::REFRESH) {
                    // refresh events are not participating in this preview 
                    continue;
                }
                if (*pchild_name == child_name && 
                    when_to_set <= event_iterator->timepoint && 
                    event_iterator->timepoint <= until_when
                ) {
                    // remove the events related to this child in between 
                    events_to_remove.push_back(event_iterator);
                }
                // preview the events  
                current_up_to_now[*pchild_name] = event_iterator->current_mA;
            }
            child_current_backup = current_up_to_now[child_name];
            // now compute what's the current right now... 
            current_up_to_now[child_name] = target_current_mA;
            for (auto &p : current_up_to_now) {
                total_currents_to_set += p.second;
            }
            // now total_currents_to_set is the current we need to set 
            
            current_up_to_now[child_name] = child_current_backup;
            
            
            // now continue the preview, until until_when 
            for ( ; event_iterator != event_queue.end(); ++event_iterator) {
                std::string *pchild_name = (std::string*)event_iterator->other_data;
                if (event_iterator->timepoint > until_when) { break; }
                if (event_iterator->func == Function::REFRESH) { continue; }
                if (*pchild_name == child_name && 
                    when_to_set <= event_iterator->timepoint && 
                    event_iterator->timepoint <= until_when
                ) {
                    // remove the events related to this child in between 
                    events_to_remove.push_back(event_iterator);
                }
                current_up_to_now[*pchild_name] = event_iterator->current_mA;
            }
            // for (auto &p : current_up_to_now) {
            //     total_currents_to_resume += p.second;
            // }
            // now total_currents_to_resume is the current we need to resume 

            for (EventQueue::iterator itr : events_to_remove) {
                if (itr->other_data) {
                    std::string *ps = (std::string*)(itr->other_data);
                    delete ps;
                }
                event_queue.erase(itr);
            }
            
            // enqueue our new events, but now at the end of this event, resume the previewed current 
            this->event_queue.emplace(
                when_to_set, 
                this->next_sequence_number(), 
                Function::SET_CURRENT, 
                target_current_mA, 
                is_greater_than_target,
                new std::string(child_name)
            );

            this->event_queue.emplace(
                until_when, 
                this->next_sequence_number(), 
                Function::SET_CURRENT_END, 
                current_up_to_now[child_name], 
                is_greater_than_target,
                new std::string(child_name)
            );

            // now forward the request to the source 
            this->source->schedule_set_current(
                total_currents_to_set, 
                is_greater_than_target, 
                when_to_set, 
                until_when
            );
        }
        cv.notify_one();
        return 1;

        // lockguard_t lkg(this->lock);
        // Scale &scale = this->scale_map[child_name];
        // Battery *source = this->source;
        // BatteryStatus source_status = source->get_status();
        // if (target_current_mA > source_status.max_discharging_current_mA * scale.max_discharge_rate || 
        //     (-target_current_mA) > source_status.max_charging_current_mA * scale.max_charge_rate) {
        //     WARNING() << ("target current too high, event not scheduled");
        //     return 0;
        // }
        // this->children_current_now[child_name] = target_current_mA;
        // int64_t new_currents = 0;
        // for (auto &p : this->children_current_now) {
        //     new_currents += p.second;
        // }
        // return source->schedule_set_current(new_currents, is_greater_than_target, when_to_set, until_when);
    }

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



#endif // ! POLICY_HPP














