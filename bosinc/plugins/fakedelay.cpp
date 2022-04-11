#include "BatteryStatus.h"
#include <stdio.h>
#include <stdlib.h> 
#include <unistd.h>
#include <chrono>
#include <thread>

extern "C" {
    void *init(void *arg); 
    void *destroy(void *init_result); 
    BatteryStatus get_status(void *init_result); 
    uint32_t set_current(void *init_result, int64_t current); 
    int64_t get_delay(void *init_result, int64_t from_mA, int64_t to_mA); 
}
static unsigned long communication_delay_ms = 0; 
void *init(void *arg) {
    communication_delay_ms = reinterpret_cast<unsigned long>(arg); 
    return (void*)communication_delay_ms;     
}
void *destroy(void *init_result) {
    printf("destroy, init_result=%lx\n", (long)init_result); 
    return (void*)0xdeadbeef; 
}
BatteryStatus get_status(void *init_result) {
    printf("get_status, init_result=%lx\n", (long)init_result); 
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(std::chrono::milliseconds(communication_delay_ms));
    BatteryStatus stat = {
        .voltage_mV = 123, 
        .current_mA = 456, 
        .capacity_mAh = 789, 
        .max_capacity_mAh = 101112, 
        .max_charging_current_mA = 131415, 
        .max_discharging_current_mA = 161718, 
        .timestamp = get_system_time_c()
    };
    return stat; 
}

uint32_t set_current(void *init_result, int64_t current) {
    printf("set_current, init_result=%lx, current=%ld\n", (long)init_result, (long)current); 
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(std::chrono::milliseconds(communication_delay_ms));
    return 1; 
}
int64_t get_delay(void *init_result, int64_t from_mA, int64_t to_mA) {
    return 0; 
}






