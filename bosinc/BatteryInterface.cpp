#include "BatteryInterface.hpp"

Battery::Battery(const std::string &name, const std::chrono::milliseconds &max_staleness_ms, bool no_thread) : 
    name(name), 
    status(),
    estimated_soc(),
    refresh_mode(RefreshMode::LAZY),
    max_staleness(max_staleness_ms),
    background_thread(),
    lock(),
    cv(),
    should_quit(false),
    event_queue()
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
        warning("unknown refresh mode");
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
    // bool should_quit = false;
    // std::chrono::milliseconds sampling_period;
    // while (true) {
    //     bat->lock.lock();
    //     should_quit = bat->should_cancel_background_refresh;
    //     sampling_period = bat->max_staleness;
    //     if (should_quit) {
    //         bat->lock.unlock();
    //         return;
    //     } 
    //     bat->status = bat->refresh();
    //     bat->lock.unlock();
    //     std::this_thread::sleep_for(sampling_period);
    // }
    // return;

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
        while (!(get_system_time() >= std::get<0>(bat->event_queue.top()) || bat->should_quit)) {
            if (
                bat->cv.wait_until(lk, 
                std::get<0>(bat->event_queue.top())
            ) == std::cv_status::timeout) { 
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
        bool has_set_current = false;
        event_t last_set_current_event;
        timepoint_t now = get_system_time();
        while (!(bat->event_queue.empty())) {
            event_t top = bat->event_queue.top();
            if (std::get<0>(top) > now) { 
                break; 
            }
            if (std::get<1>(top) == Function::REFRESH) {
                has_refresh = true;
            } else if (std::get<1>(top) == Function::SET_CURRENT) {
                has_set_current = true;
                last_set_current_event = top;
            }
            bat->event_queue.pop();
            now = get_system_time();
        }
        if (has_refresh && bat->refresh_mode == RefreshMode::ACTIVE) {
            bat->refresh();
            bat->event_queue.emplace(get_system_time()+bat->max_staleness, Function::REFRESH, int64_t(0), false);
        }
        if (has_set_current) {
            bat->set_current(std::get<2>(last_set_current_event), std::get<3>(last_set_current_event));
        }
    }
}

bool Battery::start_background_refresh() {
    using namespace std::chrono_literals;
    if (this->max_staleness < 100ms) {
        warning("maximum staleness should not be less than 100ms in background refresh mode");
        return false;
    } else {
        {
            lockguard_t lkd(this->lock);
            this->refresh_mode = RefreshMode::ACTIVE;
            // push the first refresh event
            this->event_queue.emplace(get_system_time()+max_staleness, Function::REFRESH, int64_t(0), false);
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

uint32_t Battery::schedule_set_current(int64_t target_current_mA, bool is_greater_than_target, timepoint_t when_to_set, timepoint_t until_when) {
    {
        lockguard_t lkd(this->lock);
        // enqueue the set current event and cancel current event
        this->event_queue.emplace(when_to_set, Function::SET_CURRENT, target_current_mA, is_greater_than_target);
        this->event_queue.emplace(until_when, Function::SET_CURRENT, int64_t(0), false);
    }
    cv.notify_one();
    return 1;
}

void Battery::update_estimated_soc(int32_t end_current, int32_t new_current) {
    // lockguard_t lkg(this->lock);  // this should be called by refresh in virtual battery
    int32_t begin_current = this->status.current_mA;

    timepoint_t end_time = std::chrono::system_clock::now();
    // the microseconds elapsed!
    std::chrono::duration<int64_t, std::chrono::system_clock::period> duration = end_time - c_time_to_timepoint(this->status.timestamp);
    int64_t periods = duration.count();  // on macOS, this is 1usec;
    int64_t msec = periods / (std::chrono::system_clock::period::den / 1000);  // on macOS, period::den / 1000 = 1000, this converts usec to msec
    double hours_elapsed = static_cast<double>(msec) / (3600 * 1000);

    int32_t estimated_mAh = ((begin_current + end_current) / 2) * hours_elapsed;
    
    this->estimated_soc -= estimated_mAh;

    this->status.timestamp = timepoint_to_c_time(end_time);
}

int32_t Battery::get_estimated_soc() {
    lockguard_t lkg(this->lock);
    uint32_t esoc = this->estimated_soc;
    return esoc;
}

void Battery::set_estimated_soc(int32_t new_estimated_soc) {
    lockguard_t lkg(this->lock);
    this->estimated_soc = new_estimated_soc;
    return;
}

void Battery::reset_estimated_soc() {
    this->get_status();
    {
        lockguard_t lkg(this->lock); 
        this->estimated_soc = this->status.state_of_charge_mAh;
    }
}





