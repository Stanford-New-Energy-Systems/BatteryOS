#ifndef DYNAMIC_BATTERY_HPP
#define DYNAMIC_BATTERY_HPP

#include "PhysicalBattery.hpp"

/**
 * Function Pointer Definitions
 *
 * refresh_t:     function that takes a void* as input and returns a BatteryStatus
 * destruct_t:    function that takes a void* as input and returns nothing
 * construct_t:   function that takes a void* as input and returns a void*
 * set_current_t: function that takes a void* as input and returns a bool
 *
 * The constructor function should return a pointer to the battery.
 * This pointer is then passed through to each function as a void*
 * and then typecasted within the function before calling the 
 * corresponding function. For example, refresh_t would take the 
 * battery pointer (as a void*), typecast it to the correct battery
 * pointer, before calling refresh() on the pointer to refresh the 
 * battery.
**/

typedef void (*destruct_t)(void*); 
typedef void* (*construct_t)(void*);
typedef BatteryStatus (*refresh_t)(void*);
typedef bool (*set_current_t)(void*, double); 

/**
 * Dynamic Battery Class
 * 
 * This battery is used to control physical batteries
 * by loading the constructor, destructor, set_current,
 * and refresh functions from a dynamic library.
 * 
 * @param battery:        pointer to battery returned by constructor function
 * @param destructor:     destructor of physical battery
 * @param constructor:    constructor of physical battery
 * @param refreshFunc:    refresh() function of physical battery
 * @param setCurrentFunc: set_current() function of physical battery  
 *
**/
class DynamicBattery: public PhysicalBattery {
    private:
        void*         battery;
        refresh_t     refreshFunc;
        destruct_t    destructor;
        construct_t   constructor;
        set_current_t setCurrentFunc;

    public:
        virtual ~DynamicBattery();
        DynamicBattery(void* initArgs,
                       void* destructor,
                       void* constructor,
                       void* refreshFunc,
                       void* setCurrentFunc,
                       const std::string& batteryName,
                       const std::chrono::milliseconds& maxStaleness = std::chrono::milliseconds(1000),
                       const RefreshMode& refreshMode = RefreshMode::LAZY);

    protected:
        BatteryStatus refresh() override;
        bool set_current(double current_mA) override;
};

#endif
