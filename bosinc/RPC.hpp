#ifndef RPC_HPP
#define RPC_HPP

#include "util.hpp"
#include "BatteryStatus.h"
#include "BatteryInterface.hpp"
extern "C" {
typedef struct RPCRequestHeader {
    int64_t func;
    int64_t remote_name_length;
    // the following parameters are for set_current
    int64_t current_mA;
    int64_t is_greater_than;
    CTimestamp when_to_set;
    CTimestamp until_when;
} RPCRequestHeader;
}

enum class RPCFunctionID : int64_t {
    REFRESH = 1,
    GET_STATUS = 2,
    SET_CURRENT = 3,
};

/**
 * Create a new RPC Header 
 * @param func the RPCFunctionID corresponding to the function to call
 * @param remote_name the name of the remote battery
 * @return a new RPCRequestHeader, with other fields filled with 0
 */
RPCRequestHeader new_RPC_header(RPCFunctionID func, const std::string &remote_name);

/**
 * Serialize a RPCRequestHeader in a buffer
 * @param status the pointer to the RPCRequestHeader to be serialized
 * @param buffer the pointer to the buffer, note: buffer size must be greater than 48 bytes
 * @param buffer_size the size of the buffer
 * @return the number of bytes used in the buffer 
 */
size_t RPCRequestHeader_serialize(const RPCRequestHeader *header, uint8_t *buffer, size_t buffer_size);

/**
 * Deserialize a RPCRequestHeader in a buffer
 * @param status the pointer to the RPCRequestHeader to hold the deserialized value
 * @param buffer the pointer to the buffer, note: buffer size must be greater than 48 bytes
 * @param buffer_size the size of the buffer
 * @return the number of bytes read from the buffer 
 */
size_t RPCRequestHeader_deserialize(RPCRequestHeader *header, const uint8_t *buffer, size_t buffer_size);




#endif // ! RPC_HPP


