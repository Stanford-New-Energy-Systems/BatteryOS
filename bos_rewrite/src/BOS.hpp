#ifndef BOS_HPP
#define BOS_HPP

#include <map>
#include <string>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "ProtoParameters.hpp"
#include "BatteryDirectoryManager.hpp"

/**
 * BOS
 *
 * The battery operating system manages the fifos and responds
 * to commands from users as well as listens for commands over 
 * sockets and responds over the network. A directory of batteries
 * is maintained and BOS is responsible for sending commands to 
 * these batteries when necessary. 
 *
 * @param fds:              list of file descriptors to poll
 * @param hasQuit:          makes sure shutdown is called only when necessary
 * @param quitPoll:         signal to quit polling file descriptors
 * @param fileNames:        list of fifo file names
 * @param adminFifoFD:      file descriptor of admin fifo
 * @param adminSocketFD:    file descriptor of admin socket
 * @param adminListener:    file descriptor of socket listening for admin connections 
 * @param directoryPath:    directory path where fifo files should exist 
 * @param battery_names:    map of file desriptors to battery names
 * @param batteryListener:  file descriptor of socket listening for battery connections
 * @param directoryManager: directory manager that manages batteries
 */

class BOS {
    private:
        pollfd* fds;
        bool hasQuit;
        bool quitPoll;
        int adminFifoFD;
        int adminSocketFD;
        int adminListener;
        int batteryListener;
        std::string directoryPath;
        std::unique_ptr<BatteryDirectoryManager> directoryManager;
        std::map<int, std::pair<std::string, bool>> battery_names;
        std::map<int, std::pair<std::string, std::string>> fileNames;

    public:

        /**
         * Constructors
         *
         * - Constructor requires directory path and permission if 
         *   BOS is to be ran solely in FIFO mode without communication
         *   over the network.
         */

        BOS();
        ~BOS();
        BOS(const std::string &directoryPath, mode_t permission);

    /**
     * Private Helper Functions
     *
     * @func pollFDs:                 polls the file descriptors in this->fds 
     * @func createFifos:             creates an input/output fifo for a battery
     * @func createDirectory:         creates a directory using given file path
     * @func handleAdminCommand:      handles a command from the admin fifo or admin socket 
     * @func handleBatteryCommand:    handles a battery command from a battery fifo or battery socket
     * @func checkFileDescriptors:    check file descriptors for POLLIN
     * @func acceptBatteryConnection: accepts a connection for battery communication over network 
     */

    private:
        void pollFDs();
        void checkFileDescriptors();
        void acceptBatteryConnection();
        void handleBatteryCommand(int fd);
        void handleAdminCommand(int fd, bool FIFO);
        void createDirectory(const std::string &directoryPath, mode_t permission);
        int  createFifos(const std::string& inputFile, const std::string& outputFile, mode_t permission);
    
    /**
     * Public Helper Functions
     *
     * @func shutdown:     shutdowns BOS instance (deletes fifos and closes socket)
     * @func startFifos:   creates directory and admin fifos so user can send commands
     * @func startSockets: creates admin and battery sockets so user can send commands
     */

    public:
        void shutdown();
        void startFifos(mode_t adminPermission);
        void startSockets(int adminPort, int batteryPort);

    /**
     * Private Helper Functions
     *
     * @func getStatus:          get status of battery and sends it back to user
     * @func setStatus:          sets the status of the battery 
     * @func removeBattery:      removes a battery from the directory
     * @func scheduleSetCurrent: schedules a set_current event for a battery
     */

    private:
        void getStatus(int fd);
        void removeBattery(int fd);
        void setStatus(const bosproto::BatteryCommand& command, int fd);
        void scheduleSetCurrent(const bosproto::BatteryCommand& command, int fd);

    /**
     * Private Helper Functions
     *
     * @func createPhysicalBattery:  creates a physical battery and adds to directory
     * @func createAggregateBattery: creates a aggregate battery and adds to directory 
     * @func createPartitionBattery: creates a partition battery and adds to directory 
     */

    private:
        void createPhysicalBattery(const bosproto::Admin_Command& command, int fd, bool FIFO);
        void createAggregateBattery(const bosproto::Admin_Command& command, int fd, bool FIFO);
        void createPartitionBattery(const bosproto::Admin_Command& command, int fd, bool FIFO);
};


#endif
