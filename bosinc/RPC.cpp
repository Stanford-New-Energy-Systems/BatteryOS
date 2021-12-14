#include "RPC.hpp"

RPCRequestHeader new_RPC_header(RPCFunctionID func, const std::string &remote_name) {
    RPCRequestHeader header;
    memset(&header, 0, sizeof(header));
    header.func = int64_t(func);
    header.remote_name_length = (int64_t)(remote_name.size());
    return header;
}

size_t RPCRequestHeader_serialize(const RPCRequestHeader *header, uint8_t *buffer, size_t buffer_size) {
    return serialize_struct_as_int<RPCRequestHeader, int64_t>(*header, buffer, buffer_size);
}

size_t RPCRequestHeader_deserialize(RPCRequestHeader *header, const uint8_t *buffer, size_t buffer_size) {
    return deserialize_struct_as_int<RPCRequestHeader, int64_t>(header, buffer, buffer_size);
}













