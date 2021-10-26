#include "BatteryInterface.hpp"

Battery::Battery(const std::string &name, const int64_t sampling_period) : 
    name(name), 
    background_refresh_thread(nullptr),
    lock(),
    status(),
    timestamp(get_system_time()),  // = bos.util.bos_time()
    sampling_period(sampling_period),
    estimated_soc(),
    should_background_refresh(false),
    should_cancel_background_refresh(false) 
{
}

Battery::~Battery() {
    this->stop_background_refresh();
}


BatteryStatus Battery::manual_refresh() {
    lockguard_t lkd(this->lock);
    return this->refresh();
}

const std::string &Battery::get_name() {
    return this->name;
}

void Battery::background_refresh_func(Battery *bat) {
    bool should_quit = false;
    int64_t sampling_period = 0;
    while (true) {
        bat->lock.lock();
        should_quit = bat->should_cancel_background_refresh;
        sampling_period = bat->sampling_period;
        if (should_quit) {
            bat->lock.unlock();
            return;
        } 
        bat->status = bat->refresh();
        bat->lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(sampling_period));
    }
    return;
}

bool Battery::start_background_refresh() {
    if (this->sampling_period <= 0) {
        return false;
    } else if (this->background_refresh_thread != nullptr) {
        return false;
    } else {
        this->should_background_refresh = true;
        this->background_refresh_thread = std::make_unique<std::thread>(Battery::background_refresh_func, this);
    }
    return true;
}

bool Battery::stop_background_refresh() {
    if (this->should_background_refresh) {
        // ask the refreshing thread to quit
        this->lock.lock();
        this->should_cancel_background_refresh = true;
        this->lock.unlock();
        
        // now wait for the refreshing thread to join
        this->background_refresh_thread->join();
        
        // reset the pointers
        this->background_refresh_thread.reset();
        this->should_background_refresh = false;
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
    std::chrono::duration<int64_t, std::chrono::system_clock::period> duration = end_time - this->timestamp;
    int64_t periods = duration.count();  // on macOS, this is 1usec;
    int64_t msec = periods / (std::chrono::system_clock::period::den / 1000);  // on macOS, period::den / 1000 = 1000, this converts usec to msec
    double hours_elapsed = static_cast<double>(msec) / (3600 * 1000);

    int32_t estimated_mAh = ((begin_current + end_current) / 2) * hours_elapsed;
    
    this->estimated_soc -= estimated_mAh;

    this->timestamp = end_time;
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





