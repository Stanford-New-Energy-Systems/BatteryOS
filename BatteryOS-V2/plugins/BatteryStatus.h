#ifndef BATTERY_STATUS_H
#define BATTERY_STATUS_H
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <tgmath.h>
#ifdef __cplusplus
extern "C" {
#endif
/**
 * The timestamp that is compatible with C
 * Fields:  
 *   secs_since_epoch: the number of seconds since epoch
 *   msec: the number of seconds since secs_since_epoch
 */
typedef struct CTimestamp {
    /** sec_since_epoch: integer representing the seconds since epoch */
    int64_t secs_since_epoch;
    /** msec: integer representing the milliseconds since secs_since_epoch */
    int64_t msec;
} CTimestamp;

/**
 * The status of a battery
 * Fields: 
 *   voltage_mV:                      voltage
 *   current_mA:                     current 
 *   capacity_mAh:             the current state of charge of the battery
 *   max_capacity_mAh:                max capacity of the battery
 *   max_discharging_current_mA:     max discharging current of the battery
 *   max_charging_current_mA:        max charging current of the battery
 */
typedef struct BatteryStatus {
    int64_t voltage_mV; 
    int64_t current_mA;
    int64_t capacity_mAh;
    int64_t max_capacity_mAh;
    int64_t max_charging_current_mA;
    int64_t max_discharging_current_mA;
    struct CTimestamp timestamp;
} BatteryStatus;

static inline CTimestamp get_system_time_c() {
    struct timespec tp;
    CTimestamp timestamp = {.secs_since_epoch = 0, .msec = 0};
    if (clock_gettime(CLOCK_REALTIME, &tp) != 0) {
        fprintf(stderr, "clock_gettime fails!!!");
        return timestamp;
    }
    timestamp.secs_since_epoch = tp.tv_sec;
    timestamp.msec = round(tp.tv_nsec / 1.0e6);
    if (timestamp.msec > 999) {
        timestamp.secs_since_epoch += 1;
        timestamp.msec = 0;
    }
    return timestamp;
}
/**
 * Serialize an int64_t value into buffer
 * If buffer_size < 8 then serialization fails and no serialization will be performed!
 * @param number the number to serialize
 * @param buffer the buffer to hold the serialized data
 * @param buffer_size the size of the buffer, must be > 8
 * @return the number of bytes used in buffer
 */
uint32_t serialize_int64(int64_t number, uint8_t *buffer, uint32_t buffer_size);

/**
 * Deserialize an int64_t value from buffer
 * @param buffer the buffer to deserialize, note: this must have size > 8, otherwise it's unsafe!!!
 * @return the value read from buffer
 */
int64_t deserialize_int64(const uint8_t *buffer);

/**
 * Serialize a BatteryStatus in a buffer
 * @param status the pointer to the BatteryStatus struct to be serialized
 * @param buffer the pointer to the buffer, note: buffer size must be greater than 48 bytes
 * @param buffer_size the size of the buffer
 * @return the number of bytes used in the buffer 
 */
uint32_t BatteryStatus_serialize(const struct BatteryStatus *status, uint8_t *buffer, uint32_t buffer_size);

/**
 * Deserialize a BatteryStatus in a buffer
 * @param status the pointer to the BatteryStatus struct to hold the deserialized value
 * @param buffer the pointer to the buffer, note: buffer size must be greater than 48 bytes
 * @param buffer_size the size of the buffer
 * @return the number of bytes read from the buffer 
 */
uint32_t BatteryStatus_deserialize(struct BatteryStatus *status, const uint8_t *buffer, uint32_t buffer_size);

// /**
//  * Print the buffer in hexadecimal for each byte
//  * @param buffer the buffer to print
//  * @param buffer_size the size of the buffer
//  */
// void print_buffer(uint8_t *buffer, uint32_t buffer_size);

/**
 * Compare two BatteryStatus
 * @param a pointer to status to compare
 * @param b pointer to status to compare
 * @return 1 if equal, otherwise 0
 */
int32_t BatteryStatus_compare(const BatteryStatus *a, const BatteryStatus *b);

void test_battery_status(); 

#ifdef __cplusplus
}
#endif

#endif // ! BATTERY_STATUS_H


















