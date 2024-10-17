#ifndef EVENT_T_HPP
#define EVENT_T_HPP
#include <chrono>
#include <stdint.h>
#include <set>
#include <map>
#include <vector>
#include <string>
#include <utility>

using timepoint_t = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>;


/**
 * The event IDs for BAL methods 
 * NOTE: the order of the events is used for sorting 
 * REFRESH                  : refresh status of battery; 
 * SET_CURRENT_BEGIN        : indicates when a schedule_set_current event begins; 
 * SET_CURRENT_END          : indicates when a schedule_set_current event ends; 
 * CANCEL_SET_CURRENT_EVENT : cancels all the SET_CURRENT_* events in that specific timepoint, REFRESH events are not affected
 */
enum class EventID : int {
    REFRESH                  = 0,
    SET_CURRENT_END          = 1,
    SET_CURRENT_BEGIN        = 2,
    CANCEL_SET_CURRENT_EVENT = 3,
};

/**
* struct representing a BAL event
* @param eventID:        defines kind of event
* @param eventTime:      specifies time when event happens
* @param current_mA:     target current for a SET_CURRENT_* event, set to 0 for a REFRESH event
* @param batteryName:    name of the battery that created event
* @param sequenceNumber: sequence number of event; used for ordering multiple events happening at same time
* @param sourceSequenceNumber: sequence number of event that source battery uses; used so that child can cancel event from source battery (0 if the battery does not have a source)
*/
struct event_t {
    public:
        EventID eventID;   
        double current_mA;
        timepoint_t eventTime;
        std::string batteryName;
        uint64_t sequenceNumber;

    public:
        event_t(std::string name, EventID event, int64_t current_mA, timepoint_t time, uint64_t sequenceNumber) {
            this->eventID              = event;
            this->eventTime            = time;
            this->current_mA           = current_mA;
            this->batteryName          = name;
            this->sequenceNumber       = sequenceNumber;
        }
        
 
        /**
        * Sorting BAL events
        *   1. sort events by time in which they occur
        *   2. if events occur at the same time, sort events by their eventID with higher eventIDs sorted before lower ones
        *   3. if events share the same eventID, sort events by their sequence number (should be unique for each event)
        */
        friend bool operator<(const struct event_t &lhs, const struct event_t &rhs) { 
            if (lhs.eventTime != rhs.eventTime)
                return lhs.eventTime < rhs.eventTime;
            else if (lhs.eventID != rhs.eventID)
                return lhs.eventID > rhs.eventID;
            return lhs.sequenceNumber < rhs.sequenceNumber; 
        }

        friend bool operator>(const struct event_t &lhs, const struct event_t &rhs) { 
            if (lhs.eventTime != rhs.eventTime)
                return lhs.eventTime > rhs.eventTime;
            else if (lhs.eventID != rhs.eventID)
                return lhs.eventID < rhs.eventID;
            return lhs.sequenceNumber > rhs.sequenceNumber;
        }
};

typedef std::set<event_t>    EventSet;
typedef std::vector<event_t> EventVector;
typedef std::pair<event_t, event_t> EventPair;
typedef std::map<int64_t, EventPair> EventMap;

#endif // EVENT_T_HPP
