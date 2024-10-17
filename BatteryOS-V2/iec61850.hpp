#ifndef IEC61850_HPP
#define IEC61850_HPP

#include <array>
#include <tuple>
#include <sstream>
#include <fcntl.h>
#include <errno.h>
#include <algorithm>
#include "BatteryInterface.hpp"
#include "hal_thread.h"
#include "iec61850_client.h"

class IEC61850 : public PhysicalBattery {
    public:
        /****************
         * Destructor 
         * **************/
        ~IEC61850();

        /**********************************************
         * Overridden functions from Battery Interface
         * ********************************************/
        BatteryStatus refresh() override;
        std::string get_type_string() override;
        uint32_t set_current(int64_t target_current_mA, bool is_greater_than_target, void* other_data) override;

        /*****************
         * Constructors
         * ***************/
        IEC61850(const std::string &name, std::chrono::milliseconds staleness, std::string LogicalDevice_Name, 
        std::string ZBAT_Name, std::string ZBTC_Name, std::string ZINV_Name);
        IEC61850(const std::string &name, std::chrono::milliseconds staleness, std::string LogicalDevice_Name, 
        std::string ZBAT_Name, std::string ZBTC_Name, std::string ZINV_Name, std::string hostname, int tcpPort);

    private:
        /*****************************************************************
         * Variables to estalish IED Connection and track any errors that 
         * may occur when trying to read and write data to/from server
         * ***************************************************************/
        IedConnection con;
        IedClientError error;

        /*************************************************************
         * Names of the IEC61850 Logical Device on the server as well 
         * as the names of the Logical Nodes for ZBAT, ZBTC, and ZINV
         * ***********************************************************/
        std::string LogicalDevice_Name;
        std::string ZBAT_Name, ZBTC_Name, ZINV_Name;

        /*******************************************************
         * Helper Functions to start client/server connection
         * and check errors that may occur when reading/writing
         * data to/from the server
         * *****************************************************/
        bool check_MmsValue(MmsValue* value);
        bool create_iec61850_client(std::string hostname, int tcpPort);
};

#endif