#include "ProtoParameters.hpp"

paramsPhysical parsePhysicalBattery(const bosproto::Physical_Battery& battery) {
    paramsPhysical p;

    p.name = battery.batteryname();

    if (battery.has_max_staleness())
        p.staleness = std::chrono::milliseconds(battery.max_staleness());
    else
        p.staleness = std::chrono::milliseconds(1000);

    if (battery.has_refresh_mode()) {
        if (battery.refresh_mode() == bosproto::Refresh::LAZY)
            p.refresh = RefreshMode::LAZY;
        else
            p.refresh = RefreshMode::ACTIVE;
    } else
        p.refresh = RefreshMode::LAZY;
    
    return p;
}

paramsDynamic parseDynamicBattery(const bosproto::Dynamic_Battery& battery) {
    paramsDynamic p;

    p.name = battery.batteryname();

    p.refreshFunc = battery.refresh_func();
    p.destructor  = battery.destructor_func();
    p.constructor = battery.constructor_func();
    p.setCurrentFunc = battery.set_current_func();

    if (battery.has_max_staleness())
        p.staleness = std::chrono::milliseconds(battery.max_staleness());
    else
        p.staleness = std::chrono::milliseconds(1000);

    if (battery.has_refresh_mode()) {
        if (battery.refresh_mode() == bosproto::Refresh::LAZY)
            p.refresh = RefreshMode::LAZY;
        else
            p.refresh = RefreshMode::ACTIVE;
    } else
        p.refresh = RefreshMode::LAZY;

    p.initArgs = new const char*[battery.arguments_size()];
    
    for (int i = 0; i < battery.arguments_size(); i++)
        p.initArgs[i] = battery.arguments(i).c_str();

    return p;
}

paramsAggregate parseAggregateBattery(const bosproto::Aggregate_Battery& battery) {
    paramsAggregate p;

    p.name = battery.batteryname();

    if (battery.has_max_staleness()) 
        p.staleness = std::chrono::milliseconds(battery.max_staleness());
    else
        p.staleness = std::chrono::milliseconds(1000);

    if (battery.has_refresh_mode()) {
        if (battery.refresh_mode() == bosproto::Refresh::LAZY)
            p.refresh = RefreshMode::LAZY;
        else
            p.refresh = RefreshMode::ACTIVE;
    } else
        p.refresh = RefreshMode::LAZY;

    for (int i = 0; i < battery.parentnames_size(); i++)
        p.parents.push_back(battery.parentnames(i));

    return p;
}

paramsPartition parsePartitionBattery(const bosproto::Partition_Battery& battery) {
    paramsPartition p;

    p.source = battery.sourcename();
    
    for (int i = 0; i < battery.names_size(); i++)
        p.child_names.push_back(battery.names(i));

    for (int i = 0; i < battery.scales_size(); i++) {
        bosproto::Scale scale = battery.scales(i);
        Scale s(scale.capacity_proportion(), scale.charge_proportion());
        p.proportions.push_back(s);
    }

    for (int i = 0; i < battery.max_stalenesses_size(); i++) {
        uint64_t staleness = battery.max_stalenesses(i);
        p.stalenesses.push_back(std::chrono::milliseconds(staleness));
    }

    for (int i = 0; i < battery.refresh_modes_size(); i++) {
        if (battery.refresh_modes(i) == bosproto::Refresh::LAZY)
            p.refreshModes.push_back(RefreshMode::LAZY);
        else
            p.refreshModes.push_back(RefreshMode::ACTIVE);
    }

    if (battery.policy() == bosproto::Policy::PROPORTIONAL)
        p.policy = PolicyType::PROPORTIONAL;
    else if (battery.policy() == bosproto::Policy::TRANCHED)
        p.policy = PolicyType::TRANCHED;
    else
        p.policy = PolicyType::RESERVED;

    return p;
}
