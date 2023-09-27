#ifndef STREAM_HPP
#define STREAM_HPP

#include "NetService.hpp"

class Stream : public Pollable {
    public:
        virtual size_t read(char* buffer, size_t len) = 0;
        virtual size_t write(char* buffer, size_t len) = 0;

        // TODO: should this be a template?
        virtual size_t read_exact(char* buffer, size_t len) = 0;
        virtual size_t write_exact(char* buffer, size_t len) = 0;
};

#endif
