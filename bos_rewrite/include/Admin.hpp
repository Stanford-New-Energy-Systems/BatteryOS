#ifndef ADMIN_HPP
#define ADMIN_HPP

#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "util.hpp"
#include "scale.hpp"
#include "refresh.hpp"
#include "protobuf/battery.pb.h"
#include "protobuf/battery_manager.pb.h"
#include "Socket.hpp"
#include "BatteryConnection.hpp"

/**
 * Admin Class
 * 
 * Provides functionality for writing commands to the admin fifo
 * or an admin socket over the network. A user can use this to 
 * create batteries, remove batteries, or shutdown a BOS instance.
 *
 * @param fifoMode:       indicates whether commands are sent to fifo or over network
 * @param clientSocket:   communication socket over network mode
 * @param inputFilePath:  input admin fifo (used for writing commands)
 * @param outputFilePath: output admin fifo (used for reading responses)
 */

class Admin {
    private:
        bool fifoMode;
        BatteryConnection* clientSocket;
        std::string path;

    /**
     * Constructors
     *
     * - Constructor requires port (to connect over network)
     * - Constructor requires input and output fifo paths (to write and read commands from the files)
     */

    public:
        ~Admin();
        Admin(int port);
        Admin(const std::string& path);

    /**
     * Public Helper Functions
     *
     * @func shutdown:               shutdown the BOS instance
     * @func setupClient:            setups client and connects to socket
     * @func createPhysicalBattery:  creates a physical battery
     * @func createAggregateBattery: creates an aggregate battery
     * @func createPartitionBattery: creates a partition battery
     */

    public:
        bool shutdown();
        BatteryConnection* setupClient(int port);

        bool createPhysicalBattery(const std::string &name,
                                   const std::chrono::milliseconds &maxStaleness = std::chrono::milliseconds(1000), 
                                   const RefreshMode &refreshMode = RefreshMode::LAZY);

        bool createAggregateBattery(const std::string &name,
                                    std::vector<std::string> parentNames,
                                    const std::chrono::milliseconds &maxStaleness = std::chrono::milliseconds(1000), 
                                    const RefreshMode &refreshMode = RefreshMode::LAZY);

        bool createPartitionBattery(const std::string &sourceName,
                                    const PolicyType &policyType, 
                                    std::vector<std::string> names, 
                                    std::vector<Scale> child_proportions,
                                    std::vector<std::chrono::milliseconds> maxStalenesses,
                                    std::vector<RefreshMode> refreshModes);

        bool createPartitionBattery(const std::string &sourceName,
                                    const PolicyType &policyType, 
                                    std::vector<std::string> names, 
                                    std::vector<Scale> child_proportions);

        bool createDynamicBattery(const char** initArgs,
                                  const std::string& destructor,
                                  const std::string& constructor,
                                  const std::string& refreshFunc,
                                  const std::string& setCurrentFunc,
                                  const std::string& name,
                                  const unsigned int argsLen,
                                  const std::chrono::milliseconds& maxStaleness = std::chrono::milliseconds(1000),
                                  const RefreshMode& refreshMode = RefreshMode::LAZY);
                                    
};

#endif
