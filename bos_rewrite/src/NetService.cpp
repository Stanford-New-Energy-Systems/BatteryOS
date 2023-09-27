
#include "NetService.hpp"

#include <poll.h>

void NetService::add(std::weak_ptr<Pollable> pollable) {
    this->pollables.push_back(pollable);
}

void NetService::poll() {
    int num_pollables = this->pollables.size();
    struct pollfd fds[num_pollables];


    for (int i = 0; i < num_pollables; i++) {
        auto pollable = this->pollables[i].lock();
        if (!pollable) {
            continue;
        }
        fds[i] = pollable->pollInfo();
    }

    int num_events = ::poll(fds, num_pollables, 200);

    for (int i = 0; i < num_pollables; i++) {
        if (fds[i].revents) {
            auto pollable = this->pollables[i].lock();
            if (!pollable) {
                continue;
            }
            pollable->pollHandler();
        }
    }
}
