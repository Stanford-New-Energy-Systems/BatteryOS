#ifndef BOSNODE_HPP
#define BOSNODE_HPP
#include <stdint.h>

enum class BatteryType : int {
    Physical,
    Aggregate, 
    BALSplitter,
    Splitted,
};

/**
 * A node representing a general node in BOS
 */
struct BOSNode {
protected:
    BatteryType type;
public: 
    BatteryType get_battery_type() {
        return this->type;
    }
};


#endif // ! BOSNODE_HPP

