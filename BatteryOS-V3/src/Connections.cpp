#include "Connections.hpp"

UARTConnection::UARTConnection(const std::string &device_path) : serial_fd(0), connected(false), device_path(device_path) {}

UARTConnection::UARTConnection(UARTConnection &&other) : serial_fd(other.serial_fd), connected(other.connected), device_path(std::move(other.device_path)) {
    other.serial_fd = 0;
    other.connected = false;
}

UARTConnection::~UARTConnection() {
    std::cout << "UART CONNECTION DESTRUCTOR!!!" << std::endl;

    if (serial_fd) {
        close();
    }
}

bool UARTConnection::is_connected() { return connected; }

bool UARTConnection::connect() {
    serial_fd = serialOpen(device_path.c_str(), 9600);
    if (serial_fd < 0) {
        WARNING() << "failed to open serial port " << device_path;
        // fprintf(stderr, "UARTConnection::connect: failed to open serial port %s\n", device_path.c_str());
        serial_fd = 0;
        return false;
    }
    connected = true;
    usleep(WAIT_TIME);
    return true;
}

std::vector<uint8_t> UARTConnection::read(int num_bytes) {
    int num_bytes_available = serialDataAvail(serial_fd);
    std::vector<uint8_t> result;
    if (num_bytes_available < 0) {
        WARNING() << "number of bytes available is " << num_bytes_available;
        // fprintf(stderr, "UARTConnection::read: Warning: number of bytes available is %d\n", num_bytes_available);
        return result;
    }
    result.resize(num_bytes, 0);
    int num_bytes_read = 0;
    int total_bytes_read = 0;
    while (total_bytes_read < num_bytes) {
        num_bytes_read = ::read(
                serial_fd, 
                (result.data() + total_bytes_read), 
                (num_bytes - total_bytes_read));
        if (num_bytes_read < 0) {
            WARNING() << "num_bytes_read = " << num_bytes_read << ", total_bytes_read = " << total_bytes_read;
            // fprintf(stderr, "UARTConnection::read: Warning: num_bytes_read = %d, total_bytes_read = %d\n", num_bytes_read, total_bytes_read);
            return result;
        }
        total_bytes_read += num_bytes_read;
    }
    // int byte_read = 0;
    // for (int i = 0; i < num_bytes_at_least; ++i) {
    //     byte_read = serialGetchar(serial_fd);
    //     if (byte_read == -1) {
    //         fprintf(stderr, "UARTConnection::read: Warning: read timeout, requested nbytes = %d\n", num_bytes_at_least);
    //         break;
    //     }
    //     result.push_back((uint8_t)byte_read);
    // }
    // while (serialDataAvail(serial_fd) > 0) {
    //     byte_read = serialGetchar(serial_fd);
    //     if (byte_read == -1) {
    //         fprintf(stderr, "UARTConnection::read: Warning: read timeout, requested nbytes = %d\n", num_bytes_at_least);
    //         break;
    //     }
    //     result.push_back((uint8_t)byte_read);
    // }
    return result;
}

ssize_t UARTConnection::write(const std::vector<uint8_t> &bytes) {
    ssize_t nbytes = ::write(serial_fd, bytes.data(), bytes.size());
    if (nbytes < 0) {
        WARNING() << "write fails, nbytes = " << nbytes;
        // fprintf(stderr, "UARTConnection::write: Error, nbytes = %d\n", (int)nbytes);
    } else if (nbytes < (ssize_t)bytes.size()) {
        WARNING() << "not all bytes are written, written nbytes = " << nbytes;
        // fprintf(stderr, "UARTConnection::write: Warning, not all bytes are written, written nbytes = %d\n", (int)nbytes);
    }
    usleep(WAIT_TIME);
    return nbytes;
}

void UARTConnection::close() {
    serialClose(serial_fd);
    connected = false;
    serial_fd = 0;
}

void UARTConnection::flush() {
    serialFlush(serial_fd);
    usleep(WAIT_TIME);
}
