#include "JBDBMS.hpp"
#include "TestBattery.hpp"
#include "NetworkBattery.hpp"
#include "AggregatorBattery.hpp"
#include "ProportionalPolicy.hpp"
#include "SplittedBattery.hpp"

#ifndef BOS_NAME_CHECK
#define BOS_NAME_CHECK(bos, name) \ 
    do { \
        if ((bos)->directory.name_exists(name)) { \
            WARNING() << "Battery name " << (name) << " already exists!"; \
            return NULL; \
        } \
    } while (0)
#endif  // ! BOS_NAME_CHECK

class BOS {
    BOSDirectory directory;
public: 

    Battery *make_null(
        const std::string &name, 
        int64_t voltage_mV, 
        int64_t max_staleness_ms
    ) {
        BOS_NAME_CHECK(this, name);
        std::unique_ptr<Battery> null_battery(new NullBattery(
            name, 
            voltage_mV, 
            std::chrono::milliseconds(max_staleness_ms)));
        return this->directory.add_battery(std::move(null_battery));
    }

    Battery *make_physical(const std::string &name) {
        return nullptr;
    }

    Battery *make_JBDBMS(
        const std::string &name, 
        const std::string &device_address, 
        const std::string &rd6006_address, 
        int64_t max_staleness_ms
    ) {
        BOS_NAME_CHECK(this, name);
        std::unique_ptr<Battery> jbd(new JBDBMS(
            name, 
            device_address, 
            rd6006_address, 
            std::chrono::milliseconds(max_staleness_ms)));
        return this->directory.add_battery(std::move(jbd));
    }

    Battery *make_network(
        const std::string &name, 
        const std::string &remote_name, 
        const std::string &address, 
        int port, 
        int64_t max_staleness_ms
    ) {
        BOS_NAME_CHECK(this, name);
        std::unique_ptr<Battery> network(new NetworkBattery(
            name, 
            remote_name, 
            address, 
            port, 
            std::chrono::milliseconds(max_staleness_ms)));
        return this->directory.add_battery(std::move(network));
    }

    Battery *make_aggergator(
        const std::string &name, 
        int64_t voltage_mV, 
        int64_t voltage_tolerance_mV, 
        const std::vector<std::string> &src_names, 
        int64_t max_staleness_ms
    ) {
        BOS_NAME_CHECK(this, name);
        std::unique_ptr<Battery> aggregator(
            new AggregatorBattery(
                name, 
                voltage_mV, 
                voltage_tolerance_mV, 
                src_names, 
                directory, 
                std::chrono::milliseconds(max_staleness_ms)));
        Battery *agg = this->directory.add_battery(std::move(aggregator));
        for (const std::string &src : src_names) {
            this->directory.add_edge(src, name);
        }
        return agg;
    }

    // Battery *make_proportional_policy(
    //     const std::string &name, 
    //     const std::string &src_name, 
    //     const std::string &first_battery_name,
    //     int64_t first_battery_max_staleness_ms,
    //     int64_t max_staleness_ms = 1000
    // ) {
    //     BOS_NAME_CHECK(this, name);
    //     BOS_NAME_CHECK(this, first_battery_name);
    //     if (!(this->directory.name_exists(src_name))) {
    //         WARNING() << "Battery source " << src_name << " does not exist";
    //         return nullptr;
    //     }
    //     std::unique_ptr<Battery> first_battery(
    //         new SplittedBattery(
    //             first_battery_name, 
    //             std::chrono::milliseconds(first_battery_max_staleness_ms), 
    //             name, 
    //             this->directory
    //         )
    //     );

    //     Battery *pfirst_battery = first_battery.get();

    //     this->directory.add_battery(std::move(first_battery));

    //     std::unique_ptr<Battery> policy(
    //         new ProportionalPolicy(
    //             name, 
    //             src_name, 
    //             this->directory, 
    //             pfirst_battery, 
    //             std::chrono::milliseconds(max_staleness_ms)
    //         )
    //     );
    //     Battery *ppolicy = this->directory.add_battery(std::move(policy));

    //     this->directory.add_edge(name, first_battery_name);
    //     return ppolicy;
    // }

    // Battery *fork_from(
    //     const std::string &new_battery_name,
    //     const std::string &policy_name, 
    //     const std::string &from_battery,
    //     BatteryStatus target_status
    // ) {
    //     return nullptr;
    // }



    
};





















