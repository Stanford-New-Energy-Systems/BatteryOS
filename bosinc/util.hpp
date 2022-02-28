#ifndef UTIL_HPP
#define UTIL_HPP

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include <float.h>
#include <sys/time.h>
#include <chrono>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <string>
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


class LogStream {
public: 
    LogStream(const char *func, const char *file, int line) {
        this->stream() << "------------------------------ LOG ------------------------------\n";
        this->stream() << file << ":" << line << ", " << func << ": \n";
    }
    ~LogStream() {
        this->stream() << "\n------------------------------ END ------------------------------" << std::endl;
    }
    inline std::ostream &stream() {
        return std::cout;
    }
    inline void flush() {
        this->stream() << std::endl;
        this->stream().flush();
    }
};


class WarningStream {
public: 
    WarningStream(const char *func, const char *file, int line) {
        this->stream() << "------------------------------ WARNING ------------------------------\n";
        this->stream() << "In file " <<  file << ":" << line << ", function " << func << ": \n";
    }
    ~WarningStream() {
        this->stream() << "\n-------------------------------- END --------------------------------" << std::endl;
        #if PROMOTE_WARNINGS_TO_ERRORS
        this->stream() << "Since PROMOTE_WARNINGS_TO_ERRORS is 1...\n" << "Now exitting with code 2..." << std::endl;
        exit(2);
        #endif
    }
    inline std::ostream &stream() {
        return std::cerr;
    }
    inline void flush() {
        this->stream() << std::endl;
        this->stream().flush();
    }
};

class ErrorStream {
public: 
    ErrorStream(const char *func, const char *file, int line) {
        this->stream() << "------------------------------ ERROR ------------------------------\n";
        this->stream() << "In file " <<  file << ":" << line << ", function " << func << ": \n";
    }
    ~ErrorStream() {
        this->stream() << "\n------------------------------- END -------------------------------" << std::endl;
        this->stream() << "Exitting with code 1..." << std::endl;
        exit(1);
    }
    inline std::ostream &stream() {
        return std::cerr;
    }
    inline void flush() {
        this->stream() << std::endl;
        this->stream().flush();
    }
};
#define LOG() \
    LogStream(__func__, __FILE__, __LINE__).stream()
#define WARNING() \
    WarningStream(__PRETTY_FUNCTION__, __FILE__, __LINE__).stream()
#define ERROR() \
    ErrorStream(__PRETTY_FUNCTION__, __FILE__, __LINE__).stream()

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

#define warning(...) warning_r("Warning: In file ", __FILE__, ", line ", __LINE__, ", function ", __PRETTY_FUNCTION__, ": ", __VA_ARGS__)

/* Print an error and quit, arguments should have overloaded << operators */

#define error(...) error_r("Error: In file ", __FILE__, ", line ", __LINE__, ", function ", __PRETTY_FUNCTION__, ": ", __VA_ARGS__)

class CSVOutput {
    std::ofstream csvstream;
public: 
    CSVOutput(const std::string &filename, const std::vector<std::string> &colnames={"Date & Time", "Unix_date1", "Power1"}) {
        csvstream.open(filename);
        csvstream << std::fixed << std::setprecision(3);
        for (int i = 0; i < int(colnames.size()); ++i) {
            if (i < int(colnames.size() - 1)) {
                csvstream << colnames[i] << ',';
            } else {
                csvstream << colnames[i] << std::endl;
            }
        }
    }
    ~CSVOutput() {
        csvstream.close();
    }
    std::ostream &stream() {
        return this->csvstream;
    }
};

void output_status_to_csv(CSVOutput &csv, const std::vector<BatteryStatus> &statuses); 

/**
 * Serialize a struct as a bunch of int numbers
 * @param obj the struct to be serialized
 * @param buffer the buffer to hold the serialized value
 * @param buffer_size the size of the buffer
 * @return how many bytes are used in the serialization
 */
template <typename T, typename Int>
size_t serialize_struct_as_int(const T &obj, uint8_t *buffer, size_t buffer_size) {
    size_t remaining_buffer_size = buffer_size;
    size_t used_bytes = 0;
    
    if (buffer_size < sizeof(T) || (sizeof(T) % sizeof(Int)) != 0) {
        WARNING() << ("buffer is not large enough or the size of object is not divisible by sizeof(Int)");
        return 0; 
    }

    uint8_t *byte_ptr = (uint8_t*)(&obj);

    for (size_t i = 0; i < sizeof(T); ) {
        // serialize the struct one by one, each data field is sizeof(Int)-bytes
        used_bytes = serialize_int<Int>(*((Int*)byte_ptr), buffer, remaining_buffer_size);
        if (used_bytes < sizeof(Int)) {
            WARNING() << ("used_bytes is not sizeof(Int)");
            used_bytes = sizeof(Int);
        }
        byte_ptr += used_bytes;
        buffer += used_bytes;
        remaining_buffer_size -= used_bytes;
        i += used_bytes;
    }

    return buffer_size - remaining_buffer_size;
}

/**
 * Deserialize a struct as a bunch of int numbers from a buffer
 * @param obj the pointer to the struct to hold the deserialized value
 * @param buffer the pointer to the buffer, note: buffer size must be greater than 48 bytes
 * @param buffer_size the size of the buffer
 * @return the number of bytes read from the buffer 
 */
template <typename T, typename Int>
size_t deserialize_struct_as_int(T *obj, const uint8_t *buffer, size_t buffer_size) {
    if (buffer_size < sizeof(T) || (sizeof(T) % sizeof(Int)) != 0) {
        WARNING() << ("buffer is not large enough or the size of object is not divisible by sizeof(Int)");
        return 0;
    }

    uint8_t *byte_ptr = (uint8_t*)obj;
    for (size_t i = 0; i < sizeof(T); ) {
        *((Int*)byte_ptr) = deserialize_int<Int>(buffer);
        buffer += sizeof(Int);
        byte_ptr += sizeof(Int);
        i += sizeof(Int);
    }
    return sizeof(T);
}

#ifndef DBL_EQUAL
#define DBL_EQUAL(a, b) (fabs((a) - (b)) < (DBL_EPSILON))
#endif // ! DBL_EQUAL

#ifndef FLT_EQUAL
#define FLT_EQUAL(a, b) (fabs((a) - (b)) < (FLT_EPSILON))
#endif // ! FLT_EQUAL

#ifndef UNIMPLEMENTED
#define UNIMPLEMENTED(msg) do { ERROR() << "Function " << __func__ << " is unimplemented! msg: " << msg; } while (0)
#endif // ! UNIMPLEMENTED 

#endif // ! UTIL_HPP









