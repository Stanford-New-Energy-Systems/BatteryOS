/// this file serves as a fake requester to make an HTTP request while not doing anything real 
#pragma once 

#include "BatteryInterface.hpp"
#include "curl/curl.h"

class FakeRequester : public Battery {
    uint64_t fakedelay; 
public: 
    FakeRequester(
        const std::string &name, 
        const std::chrono::milliseconds ms_staleness, 
        const std::string &url, 
        uint64_t pseudodelay 
    ) : Battery(name, ms_staleness), fakedelay(pseudodelay) {}
    BatteryStatus refresh() override {
        return {}; 
    }
    uint32_t set_current(int64_t target_current_mA, bool is_greater_than_target, void *other_data) {
        return 0; 
    }
};

