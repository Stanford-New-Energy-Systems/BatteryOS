#include "BatteryInterface.hpp"

Battery::Battery(const std::string &name, const std::chrono::milliseconds &max_staleness_ms) : 
    name(name), 
    status(),
    estimated_soc(),
    refresh_mode(RefreshMode::LAZY),
    max_staleness(max_staleness_ms),
    background_refresh_thread(nullptr),
    lock(),
    should_cancel_background_refresh(false) 
    // timestamp(get_system_time()),  // = bos.util.bos_time()
{
}

Battery::~Battery() {
    this->stop_background_refresh();
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
    lockguard_t lkg(this->lock);
    return this->max_staleness;
}

bool Battery::check_staleness_and_refresh() {
    lockguard_t lkg(this->lock);
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

void Battery::background_refresh_func(Battery *bat) {
    bool should_quit = false;
    std::chrono::milliseconds sampling_period;
    while (true) {
        bat->lock.lock();
        should_quit = bat->should_cancel_background_refresh;
        sampling_period = bat->max_staleness;
        if (should_quit) {
            bat->lock.unlock();
            return;
        } 
        bat->status = bat->refresh();
        bat->lock.unlock();
        std::this_thread::sleep_for(sampling_period);
    }
    return;
}

bool Battery::start_background_refresh() {
    using namespace std::chrono_literals;
    if (this->max_staleness < 100ms) {
        warning("maximum staleness should not be less than 100ms in background refresh mode");
        return false;
    } else if (this->background_refresh_thread != nullptr) {
        return false;
    } else {
        this->refresh_mode = RefreshMode::ACTIVE;
        this->background_refresh_thread = std::make_unique<std::thread>(Battery::background_refresh_func, this);
    }
    return true;
}

bool Battery::stop_background_refresh() {
    if (this->refresh_mode == RefreshMode::ACTIVE) {
        // ask the refreshing thread to quit
        this->lock.lock();
        this->should_cancel_background_refresh = true;
        this->lock.unlock();
        
        // now wait for the refreshing thread to join
        this->background_refresh_thread->join();
        
        // reset the pointers
        this->background_refresh_thread.reset();
        this->refresh_mode = RefreshMode::LAZY;
        this->should_cancel_background_refresh = false;

        return true;
    }
    return false;
}

void Battery::update_estimated_soc(int32_t end_current, int32_t new_current) {
    lockguard_t lkg(this->lock);
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
    lockguard_t lkg(this->lock); 
    // note: get_status() also requires the lock, so we need recursive mutex
    this->estimated_soc = this->get_status().state_of_charge_mAh;
}





