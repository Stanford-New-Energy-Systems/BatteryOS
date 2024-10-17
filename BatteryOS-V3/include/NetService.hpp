#ifndef NET_SERVICE_HPP
#define NET_SERVICE_HPP

#include <poll.h>
#include <vector>
#include <memory>

class Pollable {
    public:
        virtual struct pollfd pollInfo() = 0;
        virtual void pollHandler() = 0;
};

/**
 * The NetService class services its file descriptors,
 * and calls their polling event handler when they
 * have input data.
 */
class NetService {
    private:
        std::vector<std::weak_ptr<Pollable>> pollables;
        
    public:
        void add(std::weak_ptr<Pollable> pollable);
        void poll();
};

#endif
