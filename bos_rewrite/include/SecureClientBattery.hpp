#ifndef SECURE_CLIENT_BATTERY_HPP
#define SECURE_CLIENT_BATTERY_HPP

#include <poll.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "util.hpp"
#include "BatteryStatus.hpp"
#include "protobuf/battery.pb.h"
#include "protobuf/battery_manager.pb.h"
#include "BatteryConnection.hpp"

/**
 * Client Battery Class
 *
 * The client battery connects to a socket and sends 
 * battery commands over the network. The APIs are the
 * exact same as a physical or virtual battery.
 *
 * @param clientSocket: socket to send commands over 
 */

class SecureClientBattery {
    private:
        std::unique_ptr<BatteryConnection> connection;
        std::unique_ptr<BatteryConnection> agg_connection;
        uint32_t* schedule;
        std::vector<std::byte> bytes;

    /**
     * Constructor
     *
     * - Constructor should include port and name of the battery
     *   that as found in the battery directory.
     */

    public:
        SecureClientBattery(int bat_port, int agg_port, const std::string& batteryName);

    /**
     * Private Helper Functions
     *
     * @func setupClient: connects to battery socket and establishes connection w battery
     */

    private:
        int setupClient(int port, const std::string& batteryName);

    /**
     * Public Helper Functions
     *
     * @func getStatus:            gets the status of the battery
     * @func setBatteryStatus:     sets the status of the battery
     * @func schedule_set_current: schedules a set_current event of the battery 
     */
    
    public:
        BatteryStatus getStatus();
        bool setBatteryStatus(const BatteryStatus& status);
        bool schedule_set_current(double current_mA, uint64_t startTime, uint64_t endTime);
        bool schedule_set_current(double current_mA, timestamp_t startTime, timestamp_t endTime);
        bool submit_schedule();
};

#endif

