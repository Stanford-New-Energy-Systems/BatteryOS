
#include "BatteryConnection.hpp"

#include <arpa/inet.h>
#include "util.hpp"


// TODO: what should return value signify
int BatteryConnection::write(const google::protobuf::MessageLite& message) {
    // TODO: error handling
    uint32_t message_len = htonl(message.ByteSizeLong());
    this->stream->write_exact((char*)&message_len, 4);

    char buffer[4096];
    message.SerializeToArray(buffer, 4096);
    this->stream->write_exact(buffer, message.ByteSizeLong());

    return 1;
}

int BatteryConnection::read(google::protobuf::MessageLite& message) {
    char buffer[4096];
    char message_len_buf[4] = {0};

    // get the expected message len and read untill it is full
    while (this->stream->read_exact(message_len_buf, 4) == -1) {}
    uint32_t message_len = ntohl(*(uint32_t*)message_len_buf);
    uint32_t ret = this->stream->read_exact(buffer, message_len);
    return message.ParseFromArray(buffer, message_len);
}

struct pollfd BatteryConnection::pollInfo() {
    return this->stream->pollInfo();
}

void BatteryConnection::pollHandler() {
    this->messageReadyHandler(this);
}
