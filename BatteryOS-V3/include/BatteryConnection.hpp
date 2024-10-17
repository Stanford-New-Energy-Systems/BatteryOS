#ifndef BATTERY_CONNECTION_HPP
#define BATTERY_CONNECTION_HPP

#include "Socket.hpp"
#include <google/protobuf/message_lite.h>

/**
 * Wrapper around a socket/fd/etc. which handles handles all
 * protocol details, e.g. protobuf message serialization/deserialization,
 * encoding message lengths, etc.
 */
class BatteryConnection : public Pollable {
    public:
        std::function<void(BatteryConnection*)> messageReadyHandler; 

        // TODO: make non-pointer?
        std::unique_ptr<Stream> stream;
        BatteryConnection(std::unique_ptr<Stream> stream) : stream(std::move(stream)) {}

        // TODO: ctors, etc.
        int write(const google::protobuf::MessageLite& message);
        int read(google::protobuf::MessageLite& message);

        // Pollable
        struct pollfd pollInfo();
        void pollHandler();
};

#endif
