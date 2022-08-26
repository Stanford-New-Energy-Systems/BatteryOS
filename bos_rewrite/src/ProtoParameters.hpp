#include "protobuf/battery.pb.h"
#include "protobuf/battery_manager.pb.h"

#include <chrono>
#include "scale.hpp"
#include "refresh.hpp"

using milliseconds = std::chrono::milliseconds;

/**
 * Parses the parameters that are serialized over a  
 * network and returns a struct of those parameters.
 */

typedef struct physicalBatteryParameters {
    std::string name;
    RefreshMode refresh;
    milliseconds staleness;
} paramsPhysical;

typedef struct aggregateBatteryParameters {
    std::string name;
    RefreshMode refresh;
    milliseconds staleness;
    std::vector<std::string> parents;
} paramsAggregate;

typedef struct partitionBatteryParameters {
    PolicyType policy;
    std::string source;
    std::vector<Scale> proportions;
    std::vector<std::string> child_names;
    std::vector<RefreshMode> refreshModes;
    std::vector<milliseconds> stalenesses;
} paramsPartition;

paramsPhysical  parsePhysicalBattery(bosproto::Physical_Battery battery);
paramsAggregate parseAggregateBattery(bosproto::Aggregate_Battery battery);
paramsPartition parsePartitionBattery(bosproto::Partition_Battery battery);
