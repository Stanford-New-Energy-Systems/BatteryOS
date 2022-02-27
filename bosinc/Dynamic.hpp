#ifndef DYNAMIC_HPP
#define DYNAMIC_HPP

#include "BatteryInterface.hpp"
#include <dlfcn.h>

class DynamicBattery : public Battery {
public: 
    /// init_func: init_argument -> init_result
    typedef void*(*init_func_t)(void*);
    /// destruct_func: init_result -> void
    typedef void*(*destruct_func_t)(void*);
    /// get_status: init_result -> BatteryStatus
    typedef BatteryStatus(*get_status_func_t)(void*);
    /// set_current: (init_result, target_current_mA) -> some returnvalue
    typedef uint32_t(*set_current_func_t)(void*, int64_t);
    /// get_delay: (init_result, from_mA, to_mA) -> delay_ms
    typedef int64_t(*get_delay_func_t)(void*, int64_t, int64_t);
    static int64_t no_delay(void* dummy, int64_t from, int64_t to) {
        return 0;
    }
private: 
    void *dynamic_lib_handle;
    init_func_t        init_func;
    destruct_func_t    destruct_func;
    get_status_func_t  get_status_func;
    set_current_func_t set_current_func;
    get_delay_func_t   get_delay_func;
    bool initialized;
    void *init_result;
    void *init_argument; 
public: 
    DynamicBattery(
        const std::string &name,
        const std::chrono::milliseconds &max_staleness_ms,
        const std::string &dynamic_lib_path, 
        init_func_t init_func, 
        destruct_func_t destruct_func,
        get_status_func_t get_status_func, 
        set_current_func_t set_current_func, 
        get_delay_func_t get_delay_func, 
        void *init_result, 
        void *init_argument
    ) : 
        Battery(name, max_staleness_ms, 0),
        init_func(init_func), 
        destruct_func(destruct_func), 
        get_status_func(get_status_func), 
        set_current_func(set_current_func), 
        get_delay_func(get_delay_func),
        initialized(true),
        init_result(init_result), 
        init_argument(init_argument)
    {
    }
    ////// NOTE: DO NOT override the destructor!!! Override the cleanup function instead!!!!!! 
    // ~DynamicBattery() {
    //     if (this->initialized) {
    //         LOG() << (void*)this->destruct_func << " " << this->init_result << " " << this->init_argument; 
    //         // this->destruct_func(this->init_result);
    //         LOG() << "finished"; 
    //     }
    // }
    void cleanup() override {
        this->destruct_func(this->init_result);
        // init_argument? 
    }
    bool is_initialized() {
        return this->initialized;
    }
protected: 
    BatteryStatus refresh() override {
        if (!this->initialized) {
            return BatteryStatus();
        }
        this->status = this->get_status_func(this->init_result); 
        return this->status;
    }
    uint32_t set_current(int64_t new_current_mA, bool is_greater_than_target, void *other_data) override {
        if (!this->initialized) {
            return -1;
        }
        return this->set_current_func(this->init_result, new_current_mA);
    }
    std::chrono::milliseconds get_delay(int64_t from_mA, int64_t to_mA) override {
        int64_t delay_ms = this->get_delay_func(this->init_result, from_mA, to_mA);
        return std::chrono::milliseconds(delay_ms);
    }
}; 

#endif // ! DYNAMIC_HPP







