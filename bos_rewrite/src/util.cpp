
#include "util.hpp"
#include <errno.h>
#include <cstring>
#include <openssl/err.h>


size_t util::read_exact(int fd, char* buffer, size_t num_bytes) {
    size_t bytes_read = 0;
    while (bytes_read < num_bytes) {
        ssize_t res = read(fd, (char*)buffer + bytes_read, num_bytes - bytes_read);
        if (res == -1) {
            if (errno != 11) {
                std::cout << "errno: " << std::strerror(errno) << std::endl;
                return -1;
            }
            continue;
        }

        bytes_read += res;
    }

    return bytes_read;
}


size_t util::write_exact(int fd, char* buffer, size_t num_bytes) {
    size_t bytes_written = 0;
    while (bytes_written < num_bytes) {
        ssize_t res = write(fd, (char*)buffer+ bytes_written, num_bytes - bytes_written);
        if (res == -1) {
            if (errno != 11) {
                std::cout << "errno: " << std::strerror(errno) << std::endl;
                return -1;
            }
            continue;
        }

        bytes_written += res;
    }

    return bytes_written;
}

size_t util::SSL_read_exact(SSL* fd, char* buffer, size_t num_bytes) {
    size_t bytes_read = 0;
    while (bytes_read < num_bytes) {
        ssize_t res = SSL_read(fd, (char*)buffer + bytes_read, num_bytes - bytes_read);
        if (res < 0) {
            int err = SSL_get_error(fd, res);
            std::cout << "SSL Error: " << ERR_error_string(err, NULL) << std::endl;
            abort();
            continue;
        }

        bytes_read += res;
    }

    return bytes_read;
}

size_t util::SSL_write_exact(SSL* fd, char* buffer, size_t num_bytes) {
    size_t bytes_written = 0;
    while (bytes_written < num_bytes) {
        ssize_t res = SSL_write(fd, (char*)buffer+ bytes_written, num_bytes - bytes_written);
        if (res == -1) {
            int err =  SSL_get_error(fd, res);
            std::cout << "SSL Error: " << ERR_error_string(err, NULL) << std::endl;
            abort();
            continue;
        }

        bytes_written += res;
    }

    return bytes_written;
}
