#include "BatteryInterface.hpp"

Battery::Battery(
    const std::string &name, 
    const std::chrono::milliseconds &max_staleness_ms, 
    bool no_thread
) : 
    name(name), 
    status(),
    refresh_mode(RefreshMode::LAZY),
    max_staleness(max_staleness_ms),
    background_thread(),
    lock(),
    cv(),
    should_quit(false),
    event_queue(),
    current_sequence_number(0),
    current_now(0),
    is_greater_than_current_now(false)
{
    if (!no_thread) {
        this->background_thread = std::thread(Battery::background_func, this);
    }
}

Battery::~Battery() {
    {
        lockguard_t lkd(this->lock);
        this->should_quit = true;
    }
    cv.notify_one();
    background_thread.join(); 
}

BatteryStatus Battery::get_status() {
    lockguard_t lkg(this->lock);
    switch (this->refresh_mode) {
    case RefreshMode::ACTIVE:
        return this->status;
    case RefreshMode::LAZY:
        this->check_staleness_and_refresh();
        return this->status;
    default:
        WARNING() << ("unknown refresh mode");
        return this->status;
    }
    return this->status;
}


BatteryStatus Battery::manual_refresh() {
    lockguard_t lkg(this->lock);
    return this->refresh();
}

const std::string &Battery::get_name() {
    return this->name;
}

void Battery::set_max_staleness(const std::chrono::milliseconds &new_staleness_ms) {
    lockguard_t lkg(this->lock);
    this->max_staleness = new_staleness_ms;
    return;
}

std::chrono::milliseconds Battery::get_max_staleness() {
    // no lock
    return this->max_staleness;
}

bool Battery::check_staleness_and_refresh() {
    // no lock
    if (this->refresh_mode != RefreshMode::LAZY) {
        return false;
    }
    auto now = get_system_time();
    if ((now - c_time_to_timepoint(this->status.timestamp)) > this->max_staleness) {
        this->refresh();
        return true;
    }
    return false;
}

void Battery::background_func(Battery *bat) {
    while (true) {
        std::unique_lock<lock_t> lk(bat->lock);
        if (bat->should_quit) return;
        if (bat->event_queue.size() == 0) {
            bat->cv.wait(lk, 
                [bat]{ 
                    return bat->event_queue.size() > 0 || bat->should_quit; 
                }
            );
            if (bat->should_quit) return;
        }
        // notice that the event_queue top element may be updated 
        while (!(get_system_time() >= bat->event_queue.begin()->timepoint || bat->should_quit)) {
            if (bat->cv.wait_until(lk, bat->event_queue.begin()->timepoint) == std::cv_status::timeout) { 
                break;
            }
        }
        // bat->cv.wait_until(lk, 
        //     std::get<0>(bat->event_queue.top()), 
        //     [bat] { 
        //         return get_system_time() >= std::get<0>(bat->event_queue.top()) || bat->should_quit; 
        //     }
        // );
        if (bat->should_quit) return;
        bool has_refresh = false;
        // bool has_set_current = false;
        // bool has_set_current_end = false;
        bool has_cancel_event = false;
        std::vector<event_t> set_current_events;
        // event_t last_set_current_event;
        timepoint_t now = get_system_time();
        EventQueue::iterator top_iterator;
        while (!(bat->event_queue.empty())) {
            top_iterator = bat->event_queue.begin();
            event_t top = (*top_iterator);
            if (top.timepoint > now) { 
                break; 
            }
            switch (top.func) {
            case Function::REFRESH:
                has_refresh = true;
                break;
            case Function::SET_CURRENT_END:
                // if (!has_set_current) {
                //     has_set_current_end = true;
                //     last_set_current_event = top;
                // }
                // break;
                [[fallthrough]];
            case Function::SET_CURRENT:
                // has_set_current = true;
                // last_set_current_event = top;
                set_current_events.push_back(top);
                break;
            case Function::CANCEL_EVENT:
                // not used 
                // As an important note: 
                // the only way to insert cancel event is when a newer set_current overlaps the current events!!!
                has_cancel_event = true;
                break;
            default:
                break;
            }
            bat->event_queue.erase(top_iterator);
            now = get_system_time();
        }
        if (has_refresh && bat->refresh_mode == RefreshMode::ACTIVE) {
            bat->refresh();
            // schedule the next refresh event
            bat->event_queue.emplace(
                get_system_time()+bat->max_staleness, 
                0, // bat->next_sequence_number(), 
                Function::REFRESH, 
                int64_t(0), 
                false
            );
        }
        // oh we have special rules for splitter policy
        if (bat->type == BatteryType::SplitterPolicy) {
            // splitter policy: perform all the set current events! 
            // because each event might correspond to a different children 
            for (event_t &set_current_event : set_current_events) {
                bat->set_current(
                    set_current_event.current_mA, 
                    set_current_event.is_greater_than, 
                    set_current_event.other_data
                );
            }
            // what about the current_now and is_greater_than_current_now? 
            // we just don't use them 
        } else {
            if (!has_cancel_event && !set_current_events.empty()) {
                event_t &last_set_current_event = set_current_events.back();
                if (bat->set_current(
                        last_set_current_event.current_mA, 
                        last_set_current_event.is_greater_than,
                        last_set_current_event.other_data)
                ) {
                    bat->current_now = last_set_current_event.current_mA;
                    bat->is_greater_than_current_now = last_set_current_event.is_greater_than;
                }
            }
        }
    }
}

bool Battery::start_background_refresh() {
    using namespace std::chrono_literals;
    if (this->max_staleness < 100ms) {
        WARNING() << ("maximum staleness should not be less than 100ms in background refresh mode");
        return false;
    } else {
        {
            lockguard_t lkd(this->lock);
            this->refresh_mode = RefreshMode::ACTIVE;
            // push the first refresh event
            this->event_queue.emplace(
                get_system_time()+max_staleness, 
                0, // this->next_sequence_number(), 
                Function::REFRESH, 
                int64_t(0), 
                false);
        }
        // tell the background thread to handle the refresh event
        cv.notify_one();
    }
    return true;
}

bool Battery::stop_background_refresh() {
    if (this->refresh_mode == RefreshMode::ACTIVE) {
        // ask the refreshing thread to quit
        {
            lockguard_t lkd(this->lock);
            this->refresh_mode = RefreshMode::LAZY;
        }
        cv.notify_one();  // do we need to notify this?
        return true;
    }
    return false;
}

uint32_t Battery::schedule_set_current(
    int64_t target_current_mA, 
    bool is_greater_than_target, 
    timepoint_t when_to_set, 
    timepoint_t until_when
) {
    {
        lockguard_t lkd(this->lock);
        timepoint_t now = get_system_time();
        if (when_to_set < now) {
            WARNING() << ("when_to_set must be at least now");
            return 0;
        }
        // if there's overlapping set current events, merge the ranges! 
        // this might cause problems??? 
        // pop out the events that are earlier than when_to_set and save them

        // preview the current events from now to until_when 
        // and determine how much current to resume to 

        int64_t current_value_up_to_now = this->current_now;
        bool is_greater_than_up_to_now = this->is_greater_than_current_now;
        std::vector<EventQueue::iterator> iterators_to_remove;
        for (
            EventQueue::iterator event_iterator = event_queue.begin(); 
            event_iterator != event_queue.end(); 
            ++event_iterator
        ) {
            if (event_iterator->timepoint > until_when) {
                // now we know about what current to resume to! 
                // but, be careful when multiple events are colliding
                break;
            }
            if (event_iterator->func == Function::REFRESH) {
                // refresh events are not participating in this preview 
                continue;
            }
            // ok now we have set_current events, preview the current
            current_value_up_to_now = event_iterator->current_mA;
            is_greater_than_up_to_now = event_iterator->is_greater_than;
            // and if this event is between when_to_set and until_when, we can remove it now 
            if (
                when_to_set <= event_iterator->timepoint && 
                event_iterator->timepoint <= until_when
            ) {
                iterators_to_remove.push_back(event_iterator);
            }
        }
        // for splitter policy, we don't remove any event! 
        // because events might be from different children 
        // but splitter policy should have this function rewritten 

        // now remove all iterators in iterators_to_remove
        for (EventQueue::iterator itr : iterators_to_remove) {
            event_queue.erase(itr);
        }

        
        // enqueue our new events, but now at the end of this event, resume the previewed current 
        this->event_queue.emplace(
            when_to_set, 
            this->next_sequence_number(), 
            Function::SET_CURRENT, 
            target_current_mA, 
            is_greater_than_target
        );

        this->event_queue.emplace(
            until_when, 
            this->next_sequence_number(), 
            Function::SET_CURRENT_END, 
            current_value_up_to_now, 
            is_greater_than_up_to_now
        );

        // std::vector<event_t> events_before_time;
        // while (!this->event_queue.empty()) {
        //     if (this->event_queue.top().timepoint < when_to_set) {
        //         events_before_time.push_back(this->event_queue.top());
        //         this->event_queue.pop();
        //     } else {
        //         break;
        //     }
        // }
        // // discard the events that are between when_to_set and until_when
        // while (!this->event_queue.empty()) {
        //     if (this->event_queue.top().timepoint <= until_when) {
        //         this->event_queue.pop();
        //     } else {
        //         break;
        //     }
        // }
        // // push back the events that happen before when_to_set
        // for (auto &e : events_before_time) {
        //     this->event_queue.push(e);
        // }        
        // // now enqueue the set current event and cancel current event
        // this->event_queue.emplace(when_to_set, this->next_sequence_number(), Function::SET_CURRENT, target_current_mA, is_greater_than_target);
        // this->event_queue.emplace(until_when, this->next_sequence_number(), Function::SET_CURRENT_END, int64_t(0), false); 
    }
    cv.notify_one();
    return 1;
}


bool operator < (const Battery::event_t &lhs, const Battery::event_t &rhs) {
    if (lhs.timepoint < rhs.timepoint) return true;
    else if (lhs.timepoint > rhs.timepoint) return false;
    else if (lhs.time_added < rhs.time_added) return true;
    else if (lhs.time_added > rhs.time_added) return false;
    else if (lhs.sequence_number < rhs.sequence_number) return true;
    else if (lhs.sequence_number > rhs.sequence_number) return false;
    else if (lhs.func < rhs.func) return true;
    else return false;
}

bool operator > (const Battery::event_t &lhs, const Battery::event_t &rhs) {
    if (lhs.timepoint > rhs.timepoint) return true;
    else if (lhs.timepoint < rhs.timepoint) return false;
    else if (lhs.time_added > rhs.time_added) return true;
    else if (lhs.time_added < rhs.time_added) return false;
    else if (lhs.sequence_number > rhs.sequence_number) return true;
    else if (lhs.sequence_number < rhs.sequence_number) return false;
    else if (lhs.func > rhs.func) return true;
    else return false;
}


