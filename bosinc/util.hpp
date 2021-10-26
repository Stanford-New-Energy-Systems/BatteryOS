#ifndef UTIL_HPP
#define UTIL_HPP

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include <sys/time.h>
#include <chrono>
#include <iostream>
#include "BatteryStatus.h"
using timepoint_t = std::chrono::time_point<std::chrono::system_clock>;

#ifndef PROMOTE_WARNINGS_TO_ERRORS
#define PROMOTE_WARNINGS_TO_ERRORS 0
#endif

/** 
 * Retrives the timestamp since epoch, it's like sec_since_epoch.msec seconds since epoch
 * e.g., 1635203842.759 seconds since epoch, 
 *  where sec_since_epoch = 1635203842 and msec = 759
 * @return the CTimestamp struct as the timestamp
 */
CTimestamp get_system_time_c();

/**
 * Returns a std::chrono::time_point object representing the current system time
 * @return the timepoint of current time
 */
timepoint_t get_system_time();

/**
 * Converts chrono::time_point to seconds and milliseconds
 * @param tp the timepoint to convert
 * @return the converted CTimestamp struct
 */
CTimestamp timepoint_to_c_time(const timepoint_t &tp);

/**
 * Converts the seconds since epoch + milliseconds to chrono::time_point
 * @param timestamp the CTimestamp struct to be converted
 * @return the converted chrono::time_point
 */
timepoint_t c_time_to_timepoint(const CTimestamp &timestamp);


/**
 * Print the buffer in hexadecimal
 * @param buffer the buffer to print
 * @param buffer_size the size of the buffer
 */
void print_buffer(uint8_t *buffer, size_t buffer_size);

/**
 * The output operator for BatteryStatus
 */
std::ostream & operator <<(std::ostream &out, const BatteryStatus &status);

/**
 * Deserialize an integer type specified in T from the buffer, 
 * notice that the integer here must be stored in little-endian format.
 * @param buffer the buffer to hold the data
 * @return the deserialized integer
 */
template <typename T>
T deserialize_int(const uint8_t *buffer) {
    T result = 0;
    for (size_t i = 0; i < sizeof(T); ++i) {
        result |= ( ( (T)(*(buffer+i)) ) << (i * 8) );
    }
    return result;
}

/**
 * Serialize an integer of type T to the buffer, in little-endian format.
 * @param number the number of serialize
 * @param buffer the buffer to hold the data
 * @param buffer_size the size of the buffer, if this is less than sizeof(T), the serialization wouldn't be performed
 * @return how many bytes are used in the serialization
 */
template <typename T>
size_t serialize_int(T number, uint8_t *buffer, size_t buffer_size) {
    if (buffer_size < sizeof(T)) {
        return 0;
    }
    for (size_t i = 0; i < sizeof(T); ++i) {
        (*(buffer+i)) = (uint8_t)((number >> (i * 8)) & 0xFF);
    }
    return sizeof(T);
}


template <typename T>
void warning_r(const T &arg) {
    std::cerr << arg << std::endl;
    #if PROMOTE_WARNINGS_TO_ERRORS
    std::cerr << "Since PROMOTE_WARNINGS_TO_ERRORS is 1...\n" << "Now exitting with code 2..." << std::endl;
    exit(2);
    #endif
}

template <typename T, typename ...Ts>
void warning_r(const T &arg, Ts ...args) {
    std::cerr << arg;
    warning_r(args...);
}

template <typename T>
void error_r(const T &arg) {
    std::cerr << arg << std::endl;
    std::cerr << "Exitting with code 1..." << std::endl;
    exit(1);
}

template <typename T, typename ...Ts>
void error_r(const T &arg, Ts ...args) {
    std::cerr << arg;
    error_r(args...);
}

/* Print a warning, arguments should have overloaded << operators */

#define warning(...) warning_r("Warning: In file ", __FILE__, ", line ", __LINE__, ", function ", __func__, ": ", __VA_ARGS__)

/* Print an error and quit, arguments should have overloaded << operators */

#define error(...) error_r("Error: In file ", __FILE__, ", line ", __LINE__, ", function ", __func__, ": ", __VA_ARGS__)


#endif // ! UTIL_HPP






