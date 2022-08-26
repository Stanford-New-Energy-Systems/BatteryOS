#ifndef NODE_HPP
#define NODE_HPP
#include <stdint.h>

enum class BatteryType : int {
    Physical,
    Aggregate,
    Partition,
    PartitionManager,
};

/*  General Node in BOS Graph */

class Node {
    protected:
        BatteryType type;
    
    public:
        BatteryType getBatteryType() {
            return this->type;
        }      
};

#endif
