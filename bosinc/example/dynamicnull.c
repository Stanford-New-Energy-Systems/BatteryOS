#include "BatteryStatus.h"
#include <stdio.h>
#include <stdlib.h> 
// /// init_func: init_argument -> init_result
// typedef void*(*init_func_t)(void*);
// /// destruct_func: init_result -> void
// typedef void(*destruct_func_t)(void*);
// /// get_status: init_result -> BatteryStatus
// typedef BatteryStatus(*get_status_func_t)(void*);
// /// set_current: (init_result, target_current_mA) -> some returnvalue
// typedef uint32_t(*set_current_func_t)(void*, int64_t);
// /// get_delay: (init_result, from_mA, to_mA) -> delay_ms
// typedef int64_t(*get_delay_func_t)(void*, int64_t, int64_t);

// static void *argument;

void *init(void *arg) {
    // argument = arg; 
    printf("init, arg=%s\n", (char*)arg); 
    return (void*)0xdeadbeef; 
}


void *destroy(void *init_result) {
    printf("destroy, init_result=%llx\n", (uint64_t)init_result); 
    return (void*)0xdeadbeef; 
}


BatteryStatus get_status(void *init_result) {
    printf("get_status, init_result=%llx\n", (uint64_t)init_result); 
    BatteryStatus stat = {
        .voltage_mV = 123, 
        .current_mA = 456, 
        .capacity_mAh = 789, 
        .max_capacity_mAh = 101112, 
        .max_charging_current_mA = 131415, 
        .max_discharging_current_mA = 161718, 
        .timestamp.secs_since_epoch = 192021, 
        .timestamp.msec = 986
    };
    return stat; 
}

uint32_t set_current(void *init_result, int64_t current) {
    printf("set_current, init_result=%llx, current=%lld\n", (uint64_t)init_result, current); 
    return 1; 
}

