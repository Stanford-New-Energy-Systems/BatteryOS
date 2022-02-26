#include "ProtobufMsg.hpp" 


int test_protobuf() {
    GOOGLE_PROTOBUF_VERIFY_VERSION; 
    bosproto::BatteryStatus battery_status_msg; 
    LOG() << "battery_status_msg.IsInitialized() = " << battery_status_msg.IsInitialized(); 
    return 0; 
}
