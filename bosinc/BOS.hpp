#include "JBDBMS.hpp"
#include "TestBattery.hpp"
#include "NetworkBattery.hpp"
#include "AggregatorBattery.hpp"
#include "BALSplitter.hpp"
#include "SplittedBattery.hpp"
#include "RPC.hpp"
#include "Dynamic.hpp"
#include <unordered_map>
#include "ProtobufMsg.hpp" 
/// Battery -> make it object oriented
/// BatteryDirectory -> get batteries
/// BatteryFactory, BatteryDirectoryManager -> create/destroy batteries in the graph
/// BatteryOS -> operating system services and configuration

class BatteryOS;

class BatteryDirectoryManager {
    BOSDirectory &dir;
public:
    static std::map<std::string, void*> loaded_dynamic_libs; 
    BatteryDirectoryManager(BOSDirectory &directory) : dir(directory) {}

    /** Null battery for testing purpose */
    Battery *make_null(
        const std::string &name,
        int64_t voltage_mV,
        int64_t max_staleness_ms
    );

    /** Pseudo battery for testing purpose */
    Battery *make_pseudo(
        const std::string &name, 
        BatteryStatus status,
        int64_t max_staleness_ms
    );

    /** 
     * Creates a BAL for JBD BMS
     * @param name the name of the battery
     * @param device_address the serial port of the BMS UART interface
     * @param rd6006_address the serial port of the current regulator
     * @param max_staleness_ms the maximum staleness, in milliseconds
     * @return a pointer to the created battery, if creation failed, returns nullptr
     */
    Battery *make_JBDBMS(
        const std::string &name, 
        const std::string &device_address, 
        const std::string &rd6006_address, 
        int64_t max_staleness_ms
    );

    /**
     * Creates a network battery
     * @param name the name of the battery
     * @param remote_name the name of the remote battery
     * @param address the IP address of the remote node 
     * @param port the port number of the remote node 
     * @param max_staleness_ms the maximum staleness, in milliseconds
     * @return a pointer to the created battery, if creation failed, returns nullptr
     */
    Battery *make_networked_battery(
        const std::string &name, 
        const std::string &remote_name, 
        const std::string &address, 
        int port, 
        int64_t max_staleness_ms
    );

    /**
     * Creates an aggregator battery 
     * @param name the name of the battery
     * @param voltage_mV the voltage of the aggregated battery 
     * @param voltage_tolerance_mV the tolerance of the source battery voltage, 
     *   i.e., each source battery must have voltage voltage_mV +- voltage_tolerance_mV 
     * @param src_names the names of the source batteries 
     * @param max_staleness_ms the maximum staleness
     * @return a pointer to the created battery, if creation failed, returns nullptr
     */
    Battery *make_aggregator(
        const std::string &name, 
        int64_t voltage_mV, 
        int64_t voltage_tolerance_mV, 
        const std::vector<std::string> &src_names, 
        int64_t max_staleness_ms
    );

    /**
     * Creates a battery partitioner, 
     * and the partitoned batteries will be automatically created if success. 
     * @param name the name of the partitioner
     * @param src_name the name of the battery to be partitioned
     * @param child_names the name of the partitioned batteries 
     * @param child_scales the resource distribution of the partitioned batteries 
     * @param child_max_staleness_ms the maximum staleness for each partitioned battery 
     * @param policy_type the policy to distribute the resource 
     * @param max_staleness_ms the maximum staleness for the partitioner 
     * @return on successful creation, a vector of pointers to each partitioned battery will be created 
     *  if failed, an empty vector will be returned
     */
    std::vector<Battery*> make_splitter(
        const std::string &name, 
        const std::string &src_name, 
        const std::vector<std::string> &child_names, 
        const std::vector<Scale> &child_scales, 
        const std::vector<int64_t> &child_max_stalenesses_ms, 
        int policy_type,
        int64_t max_staleness_ms
    );

    /**
     * Creates a battery from dynamic library 
     * @param name the name of the battery 
     * @param dynamic_lib_path the path of the dynamic library 
     * @param max_staleness_ms the maximum staleness 
     * @param init_argument the argument passed to the init function 
     * @param init_func_name the name of the init function in the dynamic library 
     * @param destruct_func_name the name of the destructor in the dynamic library 
     * @param get_status_func_name the name of the get_status function in the dynamic library 
     * @param set_current_func_name the name of the set_current function in the dynamic library 
     * @param get_delay_func_name the name of the get_delay function, could be empty 
     */
    Battery *make_dynamic(
        const std::string &name, 
        const std::string &dynamic_lib_path, 
        int64_t max_staleness_ms, 
        const char *init_argument, 
        const std::string &init_func_name, 
        const std::string &destruct_func_name, 
        const std::string &get_status_func_name, 
        const std::string &set_current_func_name, 
        const std::string &get_delay_func_name = ""
    ); 
    

    /**
     * Removes a battery, specified by the name 
     */
    int remove_battery(const std::string &name);

    /**
     * Get the battery 
     */
    Battery *get_battery(const std::string &name) { return dir.get_battery(name); }
};


class BatteryOS {
    BOSDirectory dir;
    BatteryDirectoryManager mgr;
    int admin_fifo_fd; 
    std::unordered_map<std::string, int> battery_fds; 
    std::unordered_map<int, std::string> fd_to_battery_name; 
    std::string dirpath; 
public: 
    BatteryOS(const std::string &directory_path = "./bos") : dir(), mgr(dir) {
        this->dirpath = directory_path + "/"; 
    }

    BatteryDirectoryManager &get_manager() { return this->mgr; }

    void simple_remote_connection_server(int port);
    
    /** access the batteries via the filesystems (FIFOs) */
    int battery_fifo_init(mode_t permission, mode_t dir_permission);

    /** initiate the admin fifo */
    int admin_fifo_init(mode_t permission, mode_t dir_permission); 

    /** initiate fifo for single battery */
    int single_battery_fifo_init(const std::string &battery_name, mode_t permission); 

    /** the function to poll the fifos */
    int poll_fifos(); 

    /** ensure that dir_path is actually a directory, if not, create with permission */
    int ensure_dir(const std::string &dir_path, mode_t permission); 

    int handle_admin(int fd) {return 0;}
    int handle_battery(int fd) {return 0;}

private: 
    /** this one handles the battery connection request */
    int connection_handler(Connection *conn);

};


// #ifndef BOS_NAME_CHECK
// #define BOS_NAME_CHECK(bos, name)\
//     do {\
//         if ((bos)->directory.name_exists(name)) {\
//             WARNING() << "Battery name " << (name) << " already exists!";\
//             return NULL;\
//         }\
//     } while (0)
// #endif  // ! BOS_NAME_CHECK

// class BOS {
// public: 
//     BOSDirectory directory;
//     Battery *make_null(
//         const std::string &name, 
//         int64_t voltage_mV, 
//         int64_t max_staleness_ms
//     ) {
//         BOS_NAME_CHECK(this, name);
//         std::unique_ptr<Battery> null_battery(new NullBattery(
//             name, 
//             voltage_mV, 
//             std::chrono::milliseconds(max_staleness_ms)));
//         return this->directory.add_battery(std::move(null_battery));
//     }

//     Battery *make_pseudo(
//         const std::string &name, 
//         BatteryStatus status,
//         int64_t max_staleness_ms
//     ) {
//         BOS_NAME_CHECK(this, name);
//         std::unique_ptr<Battery> pseudo_battery(
//             new PseudoBattery(name, status, std::chrono::milliseconds(max_staleness_ms))
//         );
//         return this->directory.add_battery(std::move(pseudo_battery));
//     }
//     void _test_pseudo_set_status(const std::string &name, BatteryStatus new_status) {
//         Battery *bat = this->directory.get_battery(name);
//         if (!bat) {
//             WARNING() << "battery " << name << " does not exist";
//             return;
//         }
//         PseudoBattery *pbat = dynamic_cast<PseudoBattery*>(bat);
//         if (!pbat) {
//             WARNING() << "battery " << name << " is not a pseduo battery";
//             return;
//         }
//         pbat->set_status(new_status);
//     }

//     Battery *make_JBDBMS(
//         const std::string &name, 
//         const std::string &device_address, 
//         const std::string &rd6006_address, 
//         int64_t max_staleness_ms
//     ) {
//         BOS_NAME_CHECK(this, name);
//         std::unique_ptr<Battery> jbd(new JBDBMS(
//             name, 
//             device_address, 
//             rd6006_address, 
//             std::chrono::milliseconds(max_staleness_ms)));
//         return this->directory.add_battery(std::move(jbd));
//     }

//     Battery *make_network(
//         const std::string &name, 
//         const std::string &remote_name, 
//         const std::string &address, 
//         int port, 
//         int64_t max_staleness_ms
//     ) {
//         BOS_NAME_CHECK(this, name);
//         std::unique_ptr<Battery> network(new NetworkBattery(
//             name, 
//             remote_name, 
//             address, 
//             port, 
//             std::chrono::milliseconds(max_staleness_ms)));
//         return this->directory.add_battery(std::move(network));
//     }

//     Battery *make_aggregator(
//         const std::string &name, 
//         int64_t voltage_mV, 
//         int64_t voltage_tolerance_mV, 
//         const std::vector<std::string> &src_names, 
//         int64_t max_staleness_ms
//     ) {
//         BOS_NAME_CHECK(this, name);
//         std::unique_ptr<Battery> aggregator(
//             new AggregatorBattery(
//                 name, 
//                 voltage_mV, 
//                 voltage_tolerance_mV, 
//                 src_names, 
//                 this->directory, 
//                 std::chrono::milliseconds(max_staleness_ms)));
//         Battery *agg = this->directory.add_battery(std::move(aggregator));
//         for (const std::string &src : src_names) {
//             this->directory.add_edge(src, name);
//         }
//         return agg;
//     }

//     Battery *make_policy(
//         const std::string &policy_name, 
//         const std::string &src_name, 
//         const std::vector<std::string> &child_names, 
//         const std::vector<Scale> &child_scales, 
//         const std::vector<int64_t> &child_max_stalenesses_ms, 
//         int policy_type,
//         int64_t max_staleness_ms
//     ) {
//         BOS_NAME_CHECK(this, policy_name);
//         if (!this->directory.name_exists(src_name)) {
//             WARNING() << "source battery " << src_name << " does not exist";
//             return nullptr;
//         }
//         if (!(child_names.size() == child_scales.size() && child_names.size() == child_max_stalenesses_ms.size())) {
//             WARNING() << "array size mismatch";
//             return nullptr; 
//         }
//         for (const std::string &cn : child_names) {
//             if (this->directory.name_exists(cn)) {
//                 WARNING() << "child name " << cn << " already exists!";
//                 return nullptr;
//             }
//         }
//         for (size_t i = 0; i < child_names.size(); ++i) {
//             std::unique_ptr<Battery> child(new SplittedBattery(
//                 child_names[i], 
//                 this->directory, 
//                 std::chrono::milliseconds(child_max_stalenesses_ms[i])));
//             this->directory.add_battery(std::move(child));
//         }
//         std::unique_ptr<Battery> policy(new BALSplitter(
//             policy_name, 
//             src_name, 
//             this->directory, 
//             child_names, 
//             child_scales, 
//             BALSplitterType(policy_type), 
//             std::chrono::milliseconds(max_staleness_ms)
//         ));
//         BALSplitter *pptr = dynamic_cast<BALSplitter*>(policy.get());
//         this->directory.add_battery(std::move(policy));
//         this->directory.add_edge(src_name, policy_name);

//         for (const std::string &cn : child_names) {
//             this->directory.add_edge(policy_name, cn);
//             SplittedBattery *cp = dynamic_cast<SplittedBattery*>(this->directory.get_battery(cn));
//             cp->attach_to_policy(policy_name);
//         }
//         pptr->start_background_refresh();
//         return (Battery*)pptr;
//     }

//     BatteryStatus get_status(const std::string &name) {
//         Battery *bat = this->directory.get_battery(name);
//         if (!bat) {
//             WARNING() << "battery " << name << " does not exist";
//             return BatteryStatus();
//         }
//         if (bat->get_battery_type() == BatteryType::BALSplitter) {
//             WARNING() << "battery " << name << " is a splitter policy, it does not support this operation";
//             return BatteryStatus();
//         }
//         return bat->get_status();
//     }

//     uint32_t schedule_set_current(
//         const std::string &name, 
//         int64_t target_current_mA, 
//         timepoint_t at_what_time, 
//         timepoint_t until_when
//     ) {
//         Battery *bat = this->directory.get_battery(name);
//         if (!bat) {
//             WARNING() << "battery " << name << " does not exist";
//             return 0;
//         }
//         if (bat->get_battery_type() == BatteryType::BALSplitter) {
//             WARNING() << "battery " << name << " is a splitter policy, it does not support this operation";
//             return 0;
//         }
//         return bat->schedule_set_current(target_current_mA, true, at_what_time, until_when);
//     }

//     std::string get_type_string(const std::string &name) {
//         Battery *bat = this->directory.get_battery(name);
//         if (!bat) {
//             WARNING() << "battery " << name << " does not exist";
//             return "ERROR";
//         }
//         return bat->get_type_string();
//     }

//     void set_max_staleness(const std::string &name, int64_t max_staleness_ms) {
//         Battery *bat = this->directory.get_battery(name);
//         if (!bat) {
//             WARNING() << "battery " << name << " does not exist";
//             return;
//         }
//         bat->set_max_staleness(std::chrono::milliseconds(max_staleness_ms));
//     }

//     int64_t get_max_staleness(const std::string &name) {
//         Battery *bat = this->directory.get_battery(name);
//         if (!bat) {
//             WARNING() << "battery " << name << " does not exist";
//             return -1;
//         }
//         return bat->get_max_staleness().count();
//     }

//     bool start_background_refresh(const std::string &name) {
//         Battery *bat = this->directory.get_battery(name);
//         if (!bat) {
//             WARNING() << "battery " << name << " does not exist";
//             return false;
//         }
//         return bat->start_background_refresh();
//     }

//     bool stop_background_refresh(const std::string &name) {
//         Battery *bat = this->directory.get_battery(name);
//         if (!bat) {
//             WARNING() << "battery " << name << " does not exist";
//             return false;
//         }
//         if (bat->get_battery_type() == BatteryType::BALSplitter) {
//             WARNING() << "battery " << name << " is a splitter policy, it does not support turning off background refresh";
//             return false;
//         }
//         return bat->stop_background_refresh();
//     }
    

//     void simple_remote_connection_server(int port) {
//         int sockfd = socket(AF_INET, SOCK_STREAM, 0);
//         if (sockfd < 0) {
//             ERROR() << "failed to create socket";
//         }
//         sockaddr_in sockaddr;
//         sockaddr.sin_family = AF_INET;
//         sockaddr.sin_addr.s_addr = INADDR_ANY;
//         sockaddr.sin_port = htons(port);
//         if (bind(sockfd, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) < 0) {
//             ERROR() << "Failed to bind to port, errno: " << errno;
//         }
//         if (listen(sockfd, 10) < 0) {
//             ERROR() << "Failed to listen on socket. errno: " << errno;
//         }
        
//         size_t addrlen = sizeof(sockaddr);
//         int connection = accept(sockfd, (struct sockaddr*)&sockaddr, (socklen_t*)&addrlen);

//         if (connection < 0) {
//             ERROR() << "Failed to grab connection. errno: " << errno;
//         }

//         TCPConnection conn("", 0);

//         conn.port = port;
//         conn.socket_fd = connection;

//         while (true) {
//             std::vector<uint8_t> len_buf = conn.read(4);
//             if (len_buf.size() != 4) {
//                 WARNING() << "it seems like the data length is < 4! data_length = " << len_buf.size();
//             }
//             uint32_t data_len = deserialize_int<uint32_t>(len_buf.data());
//             if (data_len == 0) {
//                 ::close(connection);
//                 while (true);
//             }

//             std::vector<uint8_t> data_buf = conn.read(data_len);

//             RPCRequestHeader header;

//             size_t header_bytes = RPCRequestHeader_deserialize(&header, data_buf.data(), data_buf.size());
//             if (header_bytes != sizeof(RPCRequestHeader)) {
//                 WARNING() << "header_bytes received does not equal to the size of the header!!!";
//             }

//             std::string name;

//             for (size_t i = header_bytes; i < data_buf.size()-1; ++i) {
//                 name.push_back((char)data_buf[i]);
//             }

//             switch (header.func) {
//             case int(RPCFunctionID::REFRESH): 
//             case int(RPCFunctionID::GET_STATUS): { 
//                 BatteryStatus status = this->get_status(name);
//                 LOG() << "remote get_status: " << status;
//                 std::vector<uint8_t> buf(sizeof(uint32_t) + sizeof(BatteryStatus));
//                 serialize_int<uint32_t>(sizeof(BatteryStatus), buf.data(), buf.size());
//                 BatteryStatus_serialize(&status, buf.data() + sizeof(uint32_t), buf.size());
//                 conn.write(buf);
//             } break;
//             case int(RPCFunctionID::SET_CURRENT): {
//                 int64_t current_mA = header.current_mA;
//                 timepoint_t when_to_set = c_time_to_timepoint(header.when_to_set);
//                 timepoint_t until_when = c_time_to_timepoint(header.until_when);
//                 LOG() << "remote schedule_set_current: " << current_mA;
//                 uint32_t r = this->schedule_set_current(name, current_mA, when_to_set, until_when);
//                 std::vector<uint8_t> buf(sizeof(uint32_t) + sizeof(uint32_t));
//                 serialize_int(sizeof(uint32_t), buf.data(), buf.size());
//                 serialize_int(r, buf.data() + sizeof(uint32_t), buf.size());
//                 conn.write(buf);
//             } break;
//             default:
//                 WARNING() << "Unknown request";
//                 break;
//             }

//         }

//     }

// };























