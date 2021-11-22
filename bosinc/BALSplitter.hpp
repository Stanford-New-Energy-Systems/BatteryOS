#ifndef SPLITTER_HPP
#define SPLITTER_HPP

#include "BatteryInterface.hpp"
#include "BOSDirectory.hpp"
enum class BALSplitterType : int {
    Proportional = 0, 
    Tranche = 1, 
    Reservation = 2,
};

struct Scale {
    double capacity;
    double max_capacity;
    double max_discharge_rate;
    double max_charge_rate;
    static bool within_01_range(double num) { return 0.0 <= num && num <= 1.0; }
    Scale(double soc, double max_cap, double max_discharge_rate, double max_charge_rate) {
        if (!(within_01_range(soc) && within_01_range(max_cap) && within_01_range(max_discharge_rate) && within_01_range(max_charge_rate))) {
            WARNING() << "Scale parameter not within range [0.0, 1.0]";
            this->capacity = this->max_capacity = this->max_discharge_rate = this->max_charge_rate = 0.0;
        } else {
            this->capacity = soc;
            this->max_capacity = max_cap;
            this->max_discharge_rate = max_discharge_rate;
            this->max_charge_rate = max_charge_rate;
        }
    }
    Scale(double proportion=0.0) {
        if (!within_01_range(proportion)) {
            WARNING() << "Scale parameter not within range [0.0, 1.0]";
            this->capacity = this->max_capacity = this->max_discharge_rate = this->max_charge_rate = 0.0;
        } else {
            this->capacity = this->max_capacity = this->max_discharge_rate = this->max_charge_rate = proportion;
        }
    }
    Scale operator-(const Scale &other) {
        if (this->capacity >= other.capacity && 
            this->max_capacity >= other.max_capacity && 
            this->max_discharge_rate >= other.max_discharge_rate && 
            this->max_charge_rate >= other.max_charge_rate) 
        {
            return Scale(
                this->capacity - other.capacity, 
                this->max_capacity - other.max_capacity, 
                this->max_discharge_rate - other.max_discharge_rate, 
                this->max_charge_rate - other.max_charge_rate);
        } else {
            WARNING() << ("not enough resource to subtract!");
            return Scale(0.0);
        }
    }

    Scale operator+(const Scale &other) {
        if (within_01_range(this->capacity + other.capacity) && 
            within_01_range(this->max_capacity + other.max_capacity) && 
            within_01_range(this->max_discharge_rate + other.max_discharge_rate) && 
            within_01_range(this->max_charge_rate >= other.max_charge_rate)) 
        {
            return Scale(
                this->capacity + other.capacity, 
                this->max_capacity + other.max_capacity, 
                this->max_discharge_rate + other.max_discharge_rate, 
                this->max_charge_rate + other.max_charge_rate);
        } else {
            WARNING() << ("sum not within [0, 1] range!");
            return Scale(0.0);
        }
    }
    bool is_zero() {
        return this->capacity == 0 || this->max_capacity == 0 || this->max_discharge_rate == 0 || this->max_charge_rate == 0;
    }
};

/**
 * Policy must start background refresh after construction!!! 
 */
class BALSplitter : public VirtualBattery {
protected: 
    /** the name of the source battery */
    std::string src_name;
    /** the directory of batteries */
    BOSDirectory *pdirectory;
    /** the pointer to the source battery */
    Battery *source;
    /** the type of policy */
    BALSplitterType policy_type;


    std::vector<std::string> child_names;
    std::vector<Scale> child_scales; 

    /** the original request status of each child */
    std::vector<BatteryStatus> child_original_status;
    /** the requested currents of each child */
    std::vector<int64_t> children_current_now;
    /** the last charge estimation timepoint */
    std::vector<timepoint_t> children_last_charge_estimate_timepoint;
    /** the estimated charge for each child */
    std::vector<double> children_estimated_charge_now;
    /** the status of each children */
    std::vector<BatteryStatus> children_status_now;
    
    /** the mapping from name to its index */
    std::map<std::string, size_t> name_lookup;

    
public: 
    BALSplitter(
        const std::string &policy_name, 
        const std::string &src_name, 
        BOSDirectory &directory,
        const std::vector<std::string> &child_names, 
        const std::vector<Scale> &child_scales, 
        BALSplitterType policy_type,
        std::chrono::milliseconds max_staleness
    ) : 
        VirtualBattery(policy_name, max_staleness), 
        src_name(src_name), 
        pdirectory(&directory), 
        policy_type(policy_type),
        child_names(child_names),
        child_scales(child_scales)
    {
        this->type = BatteryType::BALSplitter;
        source = directory.get_battery(src_name);
        if (!source) { ERROR() << "source not found!"; }
        if (child_names.size() != child_scales.size()) {
            ERROR() << "number of child batteries != number of child percentages";
        }
        child_original_status.resize(child_names.size());
        children_current_now.resize(child_names.size());
        children_last_charge_estimate_timepoint.resize(child_names.size());
        children_estimated_charge_now.resize(child_names.size());
        children_status_now.resize(child_names.size());

        // check if the scales sum up to 1
        Scale total_sum(0.0);
        for (size_t i = 0; i < child_names.size(); ++i) {
            name_lookup[child_names[i]] = i;
            total_sum = total_sum + child_scales[i];
            if (total_sum.is_zero()) {
                ERROR() << "sum of child scales is not within [0, 1]";
            }
        }
        // 
        // now initialize the children status 
        BatteryStatus source_status = this->source->get_status();
        for (size_t i = 0; i < child_names.size(); ++i) {
            BatteryStatus child_status;
            
            // the set_current value now to be 0
            children_current_now[i] = 0;

            // the estimated charge of each child, initially they are reserved 
            children_estimated_charge_now[i] = source_status.capacity_mAh * child_scales[i].capacity;
            children_last_charge_estimate_timepoint[i] = get_system_time();

            // the voltage is always the source voltage, and current is always initially 0 
            child_status.voltage_mV = source_status.voltage_mV;
            child_status.current_mA = 0;

            // initially the capacity and max_capacity are reserved 
            child_status.capacity_mAh = source_status.capacity_mAh * child_scales[i].capacity;
            child_status.max_capacity_mAh = source_status.max_capacity_mAh * child_scales[i].max_capacity;

            // the max currents should be scaled 
            child_status.max_discharging_current_mA = source_status.max_discharging_current_mA * child_scales[i].max_discharge_rate;
            child_status.max_charging_current_mA = source_status.max_charging_current_mA * child_scales[i].max_charge_rate;

            // timestamp is just the source timestamp 
            child_status.timestamp = source_status.timestamp;

            // now the child status is the initial status 
            children_status_now[i] = child_status;
            
            // this is the original status requested by the virtual battery 
            child_original_status[i] = child_status;
        }
    }
protected: 
    /**
     * A refresh will update the status of all its children!  
     */
    BatteryStatus refresh() override;

    virtual BatteryStatus refresh_proportional();
    virtual BatteryStatus refresh_tranche();
    virtual BatteryStatus refresh_reservation();

    /** 
     * set current is to update the theoretic currents of its children! 
     * the real set current is already forwarded to the source battery already 
     */
    uint32_t set_current(int64_t current_mA, bool is_greater_than, void *child_vid) override {
        size_t child_id = reinterpret_cast<size_t>(child_vid);
        timepoint_t now = get_system_time();
        std::chrono::duration<double, std::ratio<3600>> hours_elapsed = 
            (now - this->children_last_charge_estimate_timepoint[child_id]);
        this->children_estimated_charge_now[child_id] -= 
            (this->children_current_now[child_id] * hours_elapsed.count());
        this->children_last_charge_estimate_timepoint[child_id] = now;
        this->children_current_now[child_id] = current_mA;
        // LOG() << "child_id: " << child_id << " current = " << current_mA << "mA";
        return 1;
    }
public: 
    /** get_status is forbidden */
    BatteryStatus get_status() override { ERROR() << "This function shouldn't be called"; return this->status; }
    /** schedule_set_current is forbidden */
    uint32_t schedule_set_current(
        int64_t, bool, timepoint_t, timepoint_t) override { ERROR() << "This function shouldn't be called"; return 0; }
    
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
        auto iter = name_lookup.find(child_name);
        if (iter == name_lookup.end()) {
            WARNING() << "battery " << child_name << " is not my child"; 
            return this->status;
        }
        size_t child_id = iter->second;
        switch (this->refresh_mode) {
        case RefreshMode::ACTIVE:
            return children_status_now[child_id];
        case RefreshMode::LAZY: 
            this->check_staleness_and_refresh();
            // WARNING() << children_status_now[child_id];
            return children_status_now[child_id];
        default: 
            WARNING() << "unknown refresh mode"; 
            return children_status_now[child_id];
        }
        return children_status_now[child_id];
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
        uint32_t success = 0;
        {
            lockguard_t lkd(this->lock);
            timepoint_t now = get_system_time();
            if (when_to_set < now) {
                WARNING() << "when_to_set must be at least now";
                return 0;
            }
            auto iter = this->name_lookup.find(child_name);
            if (iter == name_lookup.end()) {
                WARNING() << "battery " << child_name << " is not my child";
                return 0;
            }
            size_t child_id = iter->second;
            if ((target_current_mA >= 0 && target_current_mA > children_status_now[child_id].max_discharging_current_mA) || 
                (target_current_mA < 0 && (-target_current_mA) > children_status_now[child_id].max_charging_current_mA)) {
                WARNING() << "target current too high, event not scheduled" << 
                    " childname = " << child_name <<
                    ", target = " << target_current_mA << 
                    ", max_discharge = " << 
                    children_status_now[child_id].max_charging_current_mA;
                    return 0;
            }
            // if there's overlapping set current events, merge the ranges! 
            // this might cause problems??? 
            // pop out the events that are earlier than when_to_set and save them

            // preview the current events from now to until_when 
            // and determine how much current to resume to 

            decltype(this->children_current_now) current_up_to_now = this->children_current_now; // copy! 

            int64_t child_current_backup = 0;
            
            int64_t total_currents_to_set = 0;
            // int64_t total_currents_to_resume = 0;

            std::vector<EventQueue::iterator> events_to_remove;
 
            EventQueue::iterator event_iterator;
            // preview the events up to when_to_set  
            for (
                event_iterator = event_queue.begin(); 
                event_iterator != event_queue.end(); 
                ++event_iterator
            ) {
                size_t echild_id = reinterpret_cast<size_t>(event_iterator->other_data);
                if (event_iterator->timepoint > when_to_set) {
                    // now we know about the current situations on when_to_set 
                    break;
                }
                if (event_iterator->func == Function::REFRESH) {
                    // refresh events are not participating in this preview 
                    continue;
                }
                if (echild_id == child_id && 
                    when_to_set <= event_iterator->timepoint && 
                    event_iterator->timepoint <= until_when
                ) {
                    // remove the events related to this child in between 
                    events_to_remove.push_back(event_iterator);
                }
                // preview the events  
                current_up_to_now[echild_id] = event_iterator->current_mA;
            }
            child_current_backup = current_up_to_now[child_id];
            // now compute what's the current right now... 
            current_up_to_now[child_id] = target_current_mA;
            for (auto p : current_up_to_now) {
                total_currents_to_set += p;
            }
            // now total_currents_to_set is the current we need to set 
            
            current_up_to_now[child_id] = child_current_backup;
            
            std::vector<std::pair<timepoint_t, int64_t>> events_inbetween_for_others;
            // now continue the preview, until until_when 
            for ( ; event_iterator != event_queue.end(); ++event_iterator) {
                size_t echild_id = reinterpret_cast<size_t>(event_iterator->other_data);
                if (event_iterator->timepoint > until_when) { break; }
                if (event_iterator->func == Function::REFRESH) { continue; }
                current_up_to_now[echild_id] = event_iterator->current_mA;
                if (when_to_set <= event_iterator->timepoint && event_iterator->timepoint <= until_when) {
                    if (echild_id == child_id) {
                        events_to_remove.push_back(event_iterator);
                    } else if (when_to_set < event_iterator->timepoint && event_iterator->timepoint < until_when) {
                        int64_t net_currents_now = 0;
                        for (auto p : current_up_to_now) {
                            net_currents_now += p;
                        }
                        net_currents_now -= current_up_to_now[child_id];
                        net_currents_now += target_current_mA;
                        events_inbetween_for_others.push_back({event_iterator->timepoint, net_currents_now});
                    }
                }
            }
            // for (auto &p : current_up_to_now) {
            //     total_currents_to_resume += p.second;
            // }
            // now total_currents_to_resume is the current we need to resume 

            for (EventQueue::iterator itr : events_to_remove) {
                event_queue.erase(itr);
            }
            
            // enqueue our new events, but now at the end of this event, resume the previewed current 
            this->event_queue.emplace(
                when_to_set, 
                this->next_sequence_number(), 
                Function::SET_CURRENT, 
                target_current_mA, 
                is_greater_than_target,
                reinterpret_cast<void*>(child_id)
            );

            this->event_queue.emplace(
                until_when, 
                this->next_sequence_number(), 
                Function::SET_CURRENT_END, 
                current_up_to_now[child_id], 
                is_greater_than_target,
                reinterpret_cast<void*>(child_id)
            );

            // now forward the request to the source 
            success = this->source->schedule_set_current(
                total_currents_to_set, 
                is_greater_than_target, 
                when_to_set, 
                until_when
            );
            // the set-current requests for the other batteries shouldn't be distracted 
            for (auto &p : events_inbetween_for_others) {
                this->source->schedule_set_current(
                    p.second, 
                    true, 
                    p.first, 
                    until_when
                );
            }
        }
        cv.notify_one();
        return success;
    }

    // bool reset_children() {
    //     lockguard_t lkg(this->lock);
    //     BatteryStatus source_status = this->source->get_status();
    //     for (size_t i = 0; i < child_names.size(); ++i) {
    //         BatteryStatus child_status;
    //         // the estimated charge of each child, reset them to scaled source_status 
    //         children_estimated_charge_now[i] = source_status.capacity_mAh * child_scales[i].capacity;
    //         children_last_charge_estimate_timepoint[i] = get_system_time();
    //         child_status.voltage_mV = source_status.voltage_mV;
    //         // reset the capacity and max_capacity  
    //         child_status.capacity_mAh = source_status.capacity_mAh * child_scales[i].capacity;
    //         child_status.max_capacity_mAh = source_status.max_capacity_mAh * child_scales[i].max_capacity;
    //         // the max currents should be scaled 
    //         child_status.max_discharging_current_mA = source_status.max_discharging_current_mA * child_scales[i].max_discharge_rate;
    //         child_status.max_charging_current_mA = source_status.max_charging_current_mA * child_scales[i].max_charge_rate;
    //         // timestamp is just the source timestamp 
    //         child_status.timestamp = source_status.timestamp;
    //         children_status_now[i] = child_status;
    //     }
    // }



};



#endif // ! SPLITTER_HPP














