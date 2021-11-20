#include "BALSplitter.hpp"
BatteryStatus BALSplitter::refresh() {
    switch (this->policy_type) {
    case BALSplitterType::Proportional:
        this->refresh_proportional();
        break;
    case BALSplitterType::Tranche:
        this->refresh_tranche();
        break;
    case BALSplitterType::Reservation:
        this->refresh_reservation();
        break;
    default: 
        WARNING() << "Unknown policy type, falling back to proportional!";
    }
    return this->status;
}
// BatteryStatus BALSplitter::refresh() {
//     // lock should be already acquired!!! 
//     BatteryStatus source_status = this->source->get_status();
//     const std::list<Battery*> &children = this->pdirectory->get_children(this->name);
//     int64_t total_actual_charge = source_status.capacity_mAh;
//     double total_estimated_charge = 0;
//     int64_t total_net_currents = 0;
//     int64_t total_pos_currents = 0;
//     int64_t total_neg_currents = 0;

//     timepoint_t now = get_system_time();
//     std::chrono::duration<double, std::ratio<3600>> hours_elapsed;

//     for (size_t i = 0; i < this->child_names.size(); ++i) {
//         hours_elapsed = now - this->children_last_charge_estimate_timepoint[i];
//         this->children_estimated_charge_now[i] -= this->children_current_now[i] * (hours_elapsed.count());
//         this->children_last_charge_estimate_timepoint[i] = now;
//         total_estimated_charge += this->children_estimated_charge_now[i];
//         total_net_currents += this->children_current_now[i];
//         if (this->children_current_now[i] > 0) total_pos_currents += this->children_current_now[i];
//         else if (this->children_current_now[i] < 0) total_neg_currents += this->children_current_now[i];
//     }

//     int64_t source_charge_remaining = source_status.capacity_mAh;
//     int64_t source_max_charge_remaining = source_status.max_capacity_mAh;
    
//     for (Battery *c : children) {
//         std::string child_name = c->get_name();
//         size_t child_id = this->name_lookup[child_name];
//         Scale &scale = this->child_scales[child_id];

//         BatteryStatus cstatus;

//         // this is always the source voltage 
//         cstatus.voltage_mV = source_status.voltage_mV;

//         if (total_net_currents == 0) {
//             // embarrasing situation... please handle this correctly! 
//             if (source_status.current_mA > 0 && children_current_now[child_id] > 0) {
//                 cstatus.current_mA = 
//                     (double)source_status.current_mA * (double)children_current_now[child_id] / (double)total_pos_currents;
//             } else if (source_status.current_mA < 0 && children_current_now[child_id] < 0) {
//                 cstatus.current_mA = 
//                     (double)source_status.current_mA * (double)children_current_now[child_id] / (double)total_neg_currents;
//             } else {
//                 cstatus.current_mA = (children_current_now[child_id]);
//             }
//         } else {
//             // current should be proportional! (and it must be)
//             cstatus.current_mA = (int64_t)(
//                 (double)children_current_now[child_id] * (double)source_status.current_mA / (double)total_net_currents);
//         }
        
//         // now the charge status is reported according to the policy  
        
//         // this is the estimated charge according to its charge/discharge history 
//         double estimated_charge = this->children_estimated_charge_now[child_id];

//         // the charge depends on policy             
//         if (this->policy_type == BALSplitterType::Proportional) {
//             // proportional! 
//             // proportionally distribute all charges 
//             if (fabs(total_estimated_charge) < 1e-6) {
//                 // reset the charges...... because estimation failed...
//                 WARNING() << "estimation failed: total estimated charge is 0 but the source still has charge, resetting...";
//                 cstatus.capacity_mAh = total_actual_charge * child_scales[child_id].capacity;
//                 children_estimated_charge_now[child_id] = total_actual_charge * child_scales[child_id].capacity;
//             } else {
//                 cstatus.capacity_mAh = round(estimated_charge / total_estimated_charge * (double)total_actual_charge);
//             }
//             // the max_capacity should be scaled accordingly  
//             cstatus.max_capacity_mAh = source_status.max_capacity_mAh * scale.max_capacity;
//         } else {
//             // tranche and reserved
//             // charge are just the estimated charge 
//             cstatus.capacity_mAh = round(this->children_estimated_charge_now[child_id]);
//             source_charge_remaining -= cstatus.capacity_mAh;
//             // and the max capacities should be equal to their originally reserved max_capacity 
//             cstatus.max_capacity_mAh = this->child_original_status[child_id].max_capacity_mAh;
//             source_max_charge_remaining -= cstatus.max_capacity_mAh;
//         }

//         // the max currents should be scaled according to the initial scales 
//         cstatus.max_charging_current_mA = source_status.max_charging_current_mA * scale.max_charge_rate;
//         cstatus.max_discharging_current_mA = source_status.max_discharging_current_mA * scale.max_discharge_rate;
        
//         // timestamp is just the source timestamp 
//         cstatus.timestamp = source_status.timestamp;
        
//         this->children_status_now[child_id] = cstatus;
//     }
//     if (this->policy_type == BALSplitterType::Tranche || this->policy_type == BALSplitterType::Reservation) {
//         // distribute the max_charge 
//         if (source_max_charge_remaining >= 0) {
//             if (this->policy_type == BALSplitterType::Tranche)
//                 this->children_status_now.front().max_capacity_mAh += source_max_charge_remaining;
//             else if (this->policy_type == BALSplitterType::Reservation)
//                 this->children_status_now.back().max_capacity_mAh += source_max_charge_remaining;
//             source_max_charge_remaining = 0;
//         } else {
//             for (size_t cid = this->child_names.size() - 1; cid >= 0; --cid) {
//                 if (this->children_status_now[cid].max_capacity_mAh + source_max_charge_remaining <= 0) {
//                     WARNING() << "estimation failed: there are batteries with non-positive max capacity!";
//                     source_max_charge_remaining += this->children_status_now[cid].max_capacity_mAh;
//                     this->children_status_now[cid].max_capacity_mAh = 0;
//                 } else {
//                     this->children_status_now[cid].max_capacity_mAh += source_max_charge_remaining;
//                     source_max_charge_remaining = 0;
//                     break;
//                 }
//             }
//         }
//         // distribute the charge 
//         if (source_charge_remaining >= 0) {
//             if (this->policy_type == BALSplitterType::Tranche) {
//                 for (size_t cid = 0; cid < this->child_names.size(); ++cid) {
//                     if (this->children_status_now[cid].capacity_mAh + source_charge_remaining > this->children_status_now[cid].max_capacity_mAh) {
//                         source_charge_remaining -= (this->children_status_now[cid].max_capacity_mAh - this->children_status_now[cid].capacity_mAh);
//                         this->children_status_now[cid].capacity_mAh = this->children_status_now[cid].max_capacity_mAh;
//                     } else {
//                         this->children_status_now[cid].capacity_mAh += source_charge_remaining;
//                         source_charge_remaining = 0;
//                         break;
//                     }
//                 }
//             } else if (this->policy_type == BALSplitterType::Reservation) {
//                 for (size_t cid = this->child_names.size() - 1; cid >= 0; --cid) {
//                     if (this->children_status_now[cid].capacity_mAh + source_charge_remaining > this->children_status_now[cid].max_capacity_mAh) {
//                         source_charge_remaining -= (this->children_status_now[cid].max_capacity_mAh - this->children_status_now[cid].capacity_mAh);
//                         this->children_status_now[cid].capacity_mAh = this->children_status_now[cid].max_capacity_mAh;
//                     } else {
//                         this->children_status_now[cid].capacity_mAh += source_charge_remaining;
//                         source_charge_remaining = 0;
//                         break;
//                     }
//                 }
//             }
//         } else {
//             for (size_t cid = this->child_names.size() - 1; cid >= 0; --cid) {
//                 if (this->children_status_now[cid].capacity_mAh + source_charge_remaining <= 0) {
//                     LOG() << "estimation: there's a battery with non-positive capacity!";
//                     source_charge_remaining += this->children_status_now[cid].capacity_mAh;
//                     this->children_status_now[cid].capacity_mAh = 0;
//                 } else {
//                     this->children_status_now[cid].capacity_mAh += source_charge_remaining;
//                     source_charge_remaining = 0;
//                     break;
//                 }
//             }
//         }

//         if (source_charge_remaining != 0 || source_max_charge_remaining != 0) {
//             WARNING() << "the charge or max_charge remaining fail to distribute!!!";
//         }
//     }

//     return this->status;  // this return value is meaningless 
// }


BatteryStatus BALSplitter::refresh_proportional() {
    BatteryStatus source_status = this->source->get_status();
    const std::list<Battery*> &children = this->pdirectory->get_children(this->name);
    int64_t total_actual_charge = source_status.capacity_mAh;
    double total_estimated_charge = 0;
    int64_t total_net_currents = 0;
    int64_t total_pos_currents = 0;
    int64_t total_neg_currents = 0;  // in case the net currents sum up to 0...... 

    timepoint_t now = get_system_time();
    std::chrono::duration<double, std::ratio<3600>> hours_elapsed;

    for (size_t i = 0; i < this->child_names.size(); ++i) {
        hours_elapsed = now - this->children_last_charge_estimate_timepoint[i];
        this->children_estimated_charge_now[i] -= this->children_current_now[i] * (hours_elapsed.count());
        this->children_last_charge_estimate_timepoint[i] = now;
        total_estimated_charge += this->children_estimated_charge_now[i];
        total_net_currents += this->children_current_now[i];
        if (this->children_current_now[i] > 0) {
            total_pos_currents += this->children_current_now[i];
        }
        else if (this->children_current_now[i] < 0) {
            total_neg_currents += this->children_current_now[i];
        }
    }

    for (Battery *c : children) {
        std::string child_name = c->get_name();
        size_t child_id = this->name_lookup[child_name];
        Scale &scale = this->child_scales[child_id];
        BatteryStatus cstatus;
        // this is always the source voltage 
        cstatus.voltage_mV = source_status.voltage_mV;
        // currents 
        if (total_net_currents == 0) {
            if (source_status.current_mA > 0 && children_current_now[child_id] > 0) {
                cstatus.current_mA = (double)source_status.current_mA * (double)children_current_now[child_id] / (double)total_pos_currents;
            } else if (source_status.current_mA < 0 && children_current_now[child_id] < 0) {
                cstatus.current_mA = (double)source_status.current_mA * (double)children_current_now[child_id] / (double)total_neg_currents;
            } else {
                cstatus.current_mA = children_current_now[child_id];
            }
        } else {
            // current is proportional! 
            cstatus.current_mA = (int64_t)((double)children_current_now[child_id] * (double)source_status.current_mA / (double)total_net_currents);
        }
        // this is the estimated charge according to its charge/discharge history 
        double estimated_charge = this->children_estimated_charge_now[child_id];
        // proportionally distribute all charges 
        if (fabs(total_estimated_charge) < 1e-6) {
            // reset the charges...... because estimation failed...
            WARNING() << "estimation failed: total estimated charge is 0 but the source still has charge, resetting to original scale...";
            cstatus.capacity_mAh = total_actual_charge * child_scales[child_id].capacity;
            children_estimated_charge_now[child_id] = cstatus.capacity_mAh;
        } else {
            cstatus.capacity_mAh = round(estimated_charge / total_estimated_charge * (double)total_actual_charge);
        }
        // the max_capacity should be scaled accordingly  
        cstatus.max_capacity_mAh = source_status.max_capacity_mAh * scale.max_capacity;
        // the max currents should be scaled according to the initial scales 
        cstatus.max_charging_current_mA = source_status.max_charging_current_mA * scale.max_charge_rate;
        cstatus.max_discharging_current_mA = source_status.max_discharging_current_mA * scale.max_discharge_rate;
        
        // timestamp is just the source timestamp 
        cstatus.timestamp = source_status.timestamp;
        this->children_status_now[child_id] = cstatus;
    }
    return this->status;  // meaningless 
}


#define DISTRIBUTE_MAX_STATUS(FIELD, BENEFIT_IDX) \
    do { \
        if (source_status.FIELD >= 0) { \
            children_status_now[BENEFIT_IDX].FIELD += source_status.FIELD; \
        } else { \
            for (size_t i = children_status_now.size() - 1; i >= 0; --i) { \
                int64_t FIELD = children_status_now[i].FIELD + source_status.FIELD; \
                if (FIELD < 0) { \
                    LOG() << "child battery has 0 " #FIELD; \
                    source_status.FIELD += (children_status_now[i].FIELD - 1); \
                    children_status_now[i].FIELD = 1; \
                } else { \
                    children_status_now[i].FIELD += source_status.FIELD; \
                    source_status.FIELD = 0; \
                    break; \
                } \
            } \
        } \
    } while (0)

BatteryStatus BALSplitter::refresh_tranche() {
    BatteryStatus source_status = this->source->get_status();
    const std::list<Battery*> &children = this->pdirectory->get_children(this->name);
    // int64_t total_actual_charge = source_status.capacity_mAh;
    timepoint_t now = get_system_time();
    std::chrono::duration<double, std::ratio<3600>> hours_elapsed;
    
    // estimate the charge for each child now 
    for (size_t i = 0; i < this->child_names.size(); ++i) {
        hours_elapsed = now - this->children_last_charge_estimate_timepoint[i];
        this->children_estimated_charge_now[i] -= this->children_current_now[i] * (hours_elapsed.count());
        this->children_last_charge_estimate_timepoint[i] = now;
    }

    for (Battery *c : children) {
        std::string cname = c->get_name();
        size_t cid = this->name_lookup[cname];
        BatteryStatus original_status = this->child_original_status[cid];
        BatteryStatus cstatus;
        // voltage 
        cstatus.voltage_mV = source_status.voltage_mV;
        // current 
        cstatus.current_mA = children_current_now[cid];
        source_status.current_mA -= cstatus.current_mA;
        // capacity 
        cstatus.capacity_mAh = children_estimated_charge_now[cid];
        source_status.capacity_mAh -= cstatus.capacity_mAh;
        // max_capacity 
        cstatus.max_capacity_mAh = original_status.max_capacity_mAh;
        source_status.max_capacity_mAh -= cstatus.max_capacity_mAh;
        // max_charging_current 
        cstatus.max_charging_current_mA = original_status.max_charging_current_mA;
        source_status.max_charging_current_mA -= cstatus.max_charging_current_mA;
        // max_discharging_current 
        cstatus.max_discharging_current_mA = original_status.max_discharging_current_mA;
        source_status.max_discharging_current_mA -= cstatus.max_discharging_current_mA;
        // timestamp 
        cstatus.timestamp = source_status.timestamp;
        // update the status 
        children_status_now[cid] = cstatus;
        // WARNING() << children_status_now[cid];
    }

    // now we try to distribute the remaining
    // max_charging_current
    DISTRIBUTE_MAX_STATUS(max_charging_current_mA, 0);
    // max_discharging_current 
    DISTRIBUTE_MAX_STATUS(max_discharging_current_mA, 0);
    // max_capacity
    DISTRIBUTE_MAX_STATUS(max_capacity_mAh, 0);

    // current 
    if (source_status.current_mA >= 0) {
        for (size_t i = children_status_now.size()-1; i >= 0; --i) {
            if (children_status_now[i].current_mA >= 0) { 
                int64_t current_mA = children_status_now[i].current_mA + source_status.current_mA;
                if (current_mA > children_status_now[i].max_discharging_current_mA) {
                    source_status.current_mA -= (children_status_now[i].max_discharging_current_mA - children_status_now[i].current_mA);
                    children_status_now[i].current_mA = children_status_now[i].max_discharging_current_mA;
                } else {
                    children_status_now[i].current_mA += source_status.current_mA;
                    source_status.current_mA = 0;
                    break;
                }
            }
            else {
                int64_t current_mA = children_status_now[i].current_mA + source_status.current_mA;
                if (current_mA > 0) {
                    source_status.current_mA += children_status_now[i].current_mA;
                    children_status_now[i].current_mA = 0;
                } else {
                    children_status_now[i].current_mA += source_status.current_mA;
                    source_status.current_mA = 0;
                    break;
                }
            }
        }
    } else {
        for (size_t i = 0; i < children_status_now.size(); ++i) {
            if (children_status_now[i].current_mA >= 0) { 
                int64_t current_mA = children_status_now[i].current_mA + source_status.current_mA;
                if (current_mA < 0) {
                    source_status.current_mA += children_status_now[i].current_mA;
                    children_status_now[i].current_mA = 0;
                } else {
                    children_status_now[i].current_mA += source_status.current_mA;
                    source_status.current_mA = 0;
                    break;
                }
            } else {
                int64_t current_mA = children_status_now[i].current_mA + source_status.current_mA;
                if ((-current_mA) > children_status_now[i].max_charging_current_mA) {
                    source_status.current_mA += (children_status_now[i].max_charging_current_mA + children_status_now[i].current_mA);
                    children_status_now[i].current_mA = (-children_status_now[i].max_charging_current_mA);
                } else {
                    children_status_now[i].current_mA += source_status.current_mA;
                    source_status.current_mA = 0;
                    break;
                }
            }
        }
    }

    // capacity 
    if (source_status.capacity_mAh >= 0) {
        for (size_t i = 0; i < children_status_now.size(); ++i) {
            if (children_status_now[i].capacity_mAh + source_status.capacity_mAh > this->children_status_now[i].max_capacity_mAh) {
                source_status.capacity_mAh -= (children_status_now[i].max_capacity_mAh - this->children_status_now[i].capacity_mAh);
                children_status_now[i].capacity_mAh = children_status_now[i].max_capacity_mAh;
            } else {
                children_status_now[i].capacity_mAh += source_status.capacity_mAh;
                source_status.capacity_mAh = 0;
                break;
            }
        }
    } else {

        for (size_t i = children_status_now.size()-1; i >= 0; --i) {
            if (children_status_now[i].capacity_mAh + source_status.capacity_mAh <= 0) {
                LOG() << "estimation: there's a battery with non-positive capacity!";
                source_status.capacity_mAh += (children_status_now[i].capacity_mAh - 1);
                children_status_now[i].capacity_mAh = 1;
                // WARNING() << children_status_now[i];
            } else {
                // WARNING() << children_status_now[i];
                children_status_now[i].capacity_mAh += source_status.capacity_mAh;
                source_status.capacity_mAh = 0;
                break;
            }
        }
    }
    
    return this->status; // meaningless 
}

BatteryStatus BALSplitter::refresh_reservation() {
    BatteryStatus source_status = this->source->get_status();
    const std::list<Battery*> &children = this->pdirectory->get_children(this->name);
    // int64_t total_actual_charge = source_status.capacity_mAh;
    timepoint_t now = get_system_time();
    std::chrono::duration<double, std::ratio<3600>> hours_elapsed;
    
    // estimate the charge for each child now 
    for (size_t i = 0; i < this->child_names.size(); ++i) {
        hours_elapsed = now - this->children_last_charge_estimate_timepoint[i];
        this->children_estimated_charge_now[i] -= this->children_current_now[i] * (hours_elapsed.count());
        this->children_last_charge_estimate_timepoint[i] = now;
    }

    for (Battery *c : children) {
        std::string cname = c->get_name();
        size_t cid = this->name_lookup[cname];
        BatteryStatus original_status = this->child_original_status[cid];
        BatteryStatus cstatus;
        // voltage 
        cstatus.voltage_mV = source_status.voltage_mV;
        // current 
        cstatus.current_mA = children_current_now[cid];
        source_status.current_mA -= cstatus.current_mA;
        // capacity 
        cstatus.capacity_mAh = children_estimated_charge_now[cid];
        source_status.capacity_mAh -= cstatus.capacity_mAh;
        // max_capacity 
        cstatus.max_capacity_mAh = original_status.max_capacity_mAh;
        source_status.max_capacity_mAh -= cstatus.max_capacity_mAh;
        // max_charging_current 
        cstatus.max_charging_current_mA = original_status.max_charging_current_mA;
        source_status.max_charging_current_mA -= cstatus.max_charging_current_mA;
        // max_discharging_current 
        cstatus.max_discharging_current_mA = original_status.max_discharging_current_mA;
        source_status.max_discharging_current_mA -= cstatus.max_discharging_current_mA;
        // timestamp 
        cstatus.timestamp = source_status.timestamp;
        // update the status 
        children_status_now[cid] = cstatus;
    }

    // now we try to distribute the remaining
    // max_charging_current
    DISTRIBUTE_MAX_STATUS(max_charging_current_mA, children_status_now.size() - 1);
    // max_discharging_current 
    DISTRIBUTE_MAX_STATUS(max_discharging_current_mA, children_status_now.size() - 1);
    // max_capacity
    DISTRIBUTE_MAX_STATUS(max_capacity_mAh, children_status_now.size() - 1);

    // current 
    if (source_status.current_mA >= 0) {
        for (size_t i = children_status_now.size()-1; i >= 0; --i) {
            if (children_status_now[i].current_mA >= 0) { 
                int64_t current_mA = children_status_now[i].current_mA + source_status.current_mA;
                if (current_mA > children_status_now[i].max_discharging_current_mA) {
                    source_status.current_mA -= (children_status_now[i].max_discharging_current_mA - children_status_now[i].current_mA);
                    children_status_now[i].current_mA = children_status_now[i].max_discharging_current_mA;
                } else {
                    children_status_now[i].current_mA += source_status.current_mA;
                    source_status.current_mA = 0;
                    break;
                }
            } else {
                int64_t current_mA = children_status_now[i].current_mA + source_status.current_mA;
                if (current_mA > 0) {
                    source_status.current_mA += children_status_now[i].current_mA;
                    children_status_now[i].current_mA = 0;
                } else {
                    children_status_now[i].current_mA += source_status.current_mA;
                    source_status.current_mA = 0;
                    break;
                }
            }
        }
    } else {
        for (size_t i = children_status_now.size()-1; i >= 0; --i) {
            if (children_status_now[i].current_mA >= 0) { 
                int64_t current_mA = children_status_now[i].current_mA + source_status.current_mA;
                if (current_mA < 0) {
                    source_status.current_mA += children_status_now[i].current_mA;
                    children_status_now[i].current_mA = 0;
                } else {
                    children_status_now[i].current_mA += source_status.current_mA;
                    source_status.current_mA = 0;
                    break;
                }
            }
            else {
                int64_t current_mA = children_status_now[i].current_mA + source_status.current_mA;
                if ((-current_mA) > children_status_now[i].max_charging_current_mA) {
                    source_status.current_mA += (children_status_now[i].max_charging_current_mA + children_status_now[i].current_mA);
                    children_status_now[i].current_mA = (-children_status_now[i].max_charging_current_mA);
                } else {
                    children_status_now[i].current_mA += source_status.current_mA;
                    source_status.current_mA = 0;
                    break;
                }
            }
        }
    }

    // capacity 
    if (source_status.capacity_mAh >= 0) {
        for (size_t i = children_status_now.size()-1; i >= 0; --i) {
            if (children_status_now[i].capacity_mAh + source_status.capacity_mAh > this->children_status_now[i].max_capacity_mAh) {
                source_status.capacity_mAh -= (children_status_now[i].max_capacity_mAh - this->children_status_now[i].capacity_mAh);
                children_status_now[i].capacity_mAh = children_status_now[i].max_capacity_mAh;
            } else {
                children_status_now[i].capacity_mAh += source_status.capacity_mAh;
                source_status.capacity_mAh = 0;
                break;
            }
        }
    } else {
        for (size_t i = children_status_now.size()-1; i >= 0; --i) {
            if (children_status_now[i].capacity_mAh + source_status.capacity_mAh <= 0) {
                LOG() << "estimation: there's a battery with non-positive capacity!";
                source_status.capacity_mAh += (children_status_now[i].capacity_mAh - 1);
                children_status_now[i].capacity_mAh = 1;
            } else {
                children_status_now[i].capacity_mAh += source_status.capacity_mAh;
                source_status.capacity_mAh = 0;
                break;
            }
        }
    }
    
    return this->status; // meaningless 
}















