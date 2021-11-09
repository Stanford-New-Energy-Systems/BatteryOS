#include "JBDBMS.hpp"
#include "TestBattery.hpp"
#include "NetworkBattery.hpp"
#include "AggregatorBattery.hpp"
#include "Policy.hpp"
#include "SplittedBattery.hpp"

#ifndef BOS_NAME_CHECK
#define BOS_NAME_CHECK(bos, name)\
    do {\
        if ((bos)->directory.name_exists(name)) {\
            WARNING() << "Battery name " << (name) << " already exists!";\
            return NULL;\
        }\
    } while (0)
#endif  // ! BOS_NAME_CHECK

class BOS {
public: 
    BOSDirectory directory;
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

    Battery *make_pseudo(
        const std::string &name, 
        BatteryStatus status,
        int64_t max_staleness_ms
    ) {
        BOS_NAME_CHECK(this, name);
        std::unique_ptr<Battery> pseudo_battery(
            new PseudoBattery(name, status, std::chrono::milliseconds(max_staleness_ms))
        );
        return this->directory.add_battery(std::move(pseudo_battery));
    }
    void _test_pseudo_set_status(const std::string &name, BatteryStatus new_status) {
        Battery *bat = this->directory.get_battery(name);
        if (!bat) {
            WARNING() << "battery " << name << " does not exist";
            return;
        }
        PseudoBattery *pbat = dynamic_cast<PseudoBattery*>(bat);
        if (!pbat) {
            WARNING() << "battery " << name << " is not a pseduo battery";
            return;
        }
        pbat->set_status(new_status);
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
                this->directory, 
                std::chrono::milliseconds(max_staleness_ms)));
        Battery *agg = this->directory.add_battery(std::move(aggregator));
        for (const std::string &src : src_names) {
            this->directory.add_edge(src, name);
        }
        return agg;
    }

    Battery *make_policy(
        const std::string &policy_name, 
        const std::string &src_name, 
        const std::vector<std::string> &child_names, 
        const std::vector<Scale> &child_scales, 
        const std::vector<int64_t> &child_max_stalenesses_ms, 
        int policy_type,
        int64_t max_staleness_ms
    ) {
        BOS_NAME_CHECK(this, policy_name);
        if (!this->directory.name_exists(src_name)) {
            WARNING() << "source battery " << src_name << " does not exist";
            return nullptr;
        }
        if (!(child_names.size() == child_scales.size() && child_names.size() == child_max_stalenesses_ms.size())) {
            WARNING() << "array size mismatch";
            return nullptr; 
        }
        for (const std::string &cn : child_names) {
            if (this->directory.name_exists(cn)) {
                WARNING() << "child name " << cn << " already exists!";
                return nullptr;
            }
        }
        for (size_t i = 0; i < child_names.size(); ++i) {
            std::unique_ptr<Battery> child(new SplittedBattery(
                child_names[i], 
                this->directory, 
                std::chrono::milliseconds(child_max_stalenesses_ms[i])));
            this->directory.add_battery(std::move(child));
        }
        std::unique_ptr<Battery> policy(new SplitterPolicy(
            policy_name, 
            src_name, 
            this->directory, 
            child_names, 
            child_scales, 
            SplitterPolicyType(policy_type), 
            std::chrono::milliseconds(max_staleness_ms)
        ));
        SplitterPolicy *pptr = dynamic_cast<SplitterPolicy*>(policy.get());
        this->directory.add_battery(std::move(policy));
        this->directory.add_edge(src_name, policy_name);

        for (const std::string &cn : child_names) {
            this->directory.add_edge(policy_name, cn);
            SplittedBattery *cp = dynamic_cast<SplittedBattery*>(this->directory.get_battery(cn));
            cp->attach_to_policy(policy_name);
        }
        pptr->start_background_refresh();
        return (Battery*)pptr;
    }

    BatteryStatus get_status(const std::string &name) {
        Battery *bat = this->directory.get_battery(name);
        if (!bat) {
            WARNING() << "battery " << name << " does not exist";
            return BatteryStatus();
        }
        if (bat->get_battery_type() == BatteryType::SplitterPolicy) {
            WARNING() << "battery " << name << " is a splitter policy, it does not support this operation";
            return BatteryStatus();
        }
        return bat->get_status();
    }

    uint32_t schedule_set_current(
        const std::string &name, 
        int64_t target_current_mA, 
        timepoint_t at_what_time, 
        timepoint_t until_when
    ) {
        Battery *bat = this->directory.get_battery(name);
        if (!bat) {
            WARNING() << "battery " << name << " does not exist";
            return 0;
        }
        if (bat->get_battery_type() == BatteryType::SplitterPolicy) {
            WARNING() << "battery " << name << " is a splitter policy, it does not support this operation";
            return 0;
        }
        return bat->schedule_set_current(target_current_mA, true, at_what_time, until_when);
    }

    std::string get_type_string(const std::string &name) {
        Battery *bat = this->directory.get_battery(name);
        if (!bat) {
            WARNING() << "battery " << name << " does not exist";
            return "ERROR";
        }
        return bat->get_type_string();
    }

    void set_max_staleness(const std::string &name, int64_t max_staleness_ms) {
        Battery *bat = this->directory.get_battery(name);
        if (!bat) {
            WARNING() << "battery " << name << " does not exist";
            return;
        }
        bat->set_max_staleness(std::chrono::milliseconds(max_staleness_ms));
    }

    int64_t get_max_staleness(const std::string &name) {
        Battery *bat = this->directory.get_battery(name);
        if (!bat) {
            WARNING() << "battery " << name << " does not exist";
            return -1;
        }
        return bat->get_max_staleness().count();
    }

    bool start_background_refresh(const std::string &name) {
        Battery *bat = this->directory.get_battery(name);
        if (!bat) {
            WARNING() << "battery " << name << " does not exist";
            return false;
        }
        return bat->start_background_refresh();
    }

    bool stop_background_refresh(const std::string &name) {
        Battery *bat = this->directory.get_battery(name);
        if (!bat) {
            WARNING() << "battery " << name << " does not exist";
            return false;
        }
        if (bat->get_battery_type() == BatteryType::SplitterPolicy) {
            WARNING() << "battery " << name << " is a splitter policy, it does not support turning off background refresh";
            return false;
        }
        return bat->stop_background_refresh();
    }
    
};























