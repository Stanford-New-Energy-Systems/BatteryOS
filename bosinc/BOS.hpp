#ifndef BOS_HPP 
#define BOS_HPP 
#include "BatteryInterface.hpp"
#include "BOSDirectory.hpp"
#include "Connections.hpp"
#include "AggregatorBattery.hpp"
#include "BALSplitter.hpp"
#include "SplittedBattery.hpp"
#include <unordered_map>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <poll.h>
/// Battery -> make it object oriented
/// BatteryDirectory -> get batteries
/// BatteryFactory, BatteryDirectoryManager -> create/destroy batteries in the graph
/// BatteryOS -> operating system services and configuration

class BatteryOS;

class BatteryDirectoryManager {
    BOSDirectory *dir;
    BatteryOS *bos; 
public:
    BatteryDirectoryManager() {}
    BatteryDirectoryManager(BOSDirectory *directory, BatteryOS *bos) : dir(directory), bos(bos) {}

    void init(BOSDirectory *directory, BatteryOS *bos) {
        this->dir = directory;
        this->bos = bos; 
    }

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
        void *init_argument, 
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
    Battery *get_battery(const std::string &name) { return dir->get_battery(name); }
};


class BatteryOS {
    BOSDirectory dir;
    BatteryDirectoryManager mgr;
    std::string dirpath; 
    std::map<std::string, int> battery_ifds; 
    std::map<std::string, void*> loaded_dynamic_libs; 
    int admin_fifo_ifd; 
    mode_t dir_permission; 
    mode_t admin_fifo_permission; 
    mode_t battery_fifo_permission; 
    int should_quit; 
    int quitted; 
public: 
    BatteryOS() : should_quit(0), quitted(0) {
        this->mgr.init(&(this->dir), this);
    }
    ~BatteryOS() {
        if (!(this->quitted)) {
            this->shutdown(); 
        }
    }
    
    void notify_should_quit() {
        this->should_quit = 1; 
    }

    BatteryDirectoryManager &get_manager() { 
        return this->mgr; 
    }

    const std::string &get_dirpath() {
        return this->dirpath; 
    }
    /** bootup the battery os */
    int bootup_fifo(
        const std::string &directory_path = "bosdir", 
        mode_t dir_permission = 0777, 
        mode_t admin_fifo_permission = 0777,
        mode_t battery_fifo_permission = 0777
    ); 
    /** the function to poll the fifos */
    int poll_fifos(); 

    int bootup_tcp_socket(int port);

    void *find_lib(const std::string &path) {
        auto iter = this->loaded_dynamic_libs.find(path);
        if (iter != this->loaded_dynamic_libs.end()) {
            return iter->second;
        } else {
            return nullptr; 
        }
    }
    void add_lib(const std::string &path, void *lib_handle) {
        this->loaded_dynamic_libs.insert({path, lib_handle}); 
    }
    // bool remove_lib(const std::string &path) {
    //     auto iter = this->loaded_dynamic_libs.find(path);
    //     if (iter == this->loaded_dynamic_libs.end()) {
    //         return false; 
    //     }
    //     dlclose(iter->second); 
    //     this->loaded_dynamic_libs.erase(iter); 
    //     return true; 
    // }

    void simple_remote_connection_server(int port);

    /** call right after a battery is created */
    int battery_post_creation(const std::string &battery_name); 

    

private: 
    /** this one handles the battery connection request */
    int connection_handler(Connection *conn);
 
    /** initiate fifo for single battery */
    int single_battery_fifo_init(const std::string &battery_name); 

    /** initiate the admin fifo */
    int admin_fifo_init(); 

    /** access the batteries via the filesystems (FIFOs) */
    int battery_fifo_init();
    
    /** ensure that dir_path is actually a directory, if not, create with permission */
    int ensure_dir(const std::string &dir_path, mode_t permission); 

    /** handle admin message from file descriptor fd */
    static int handle_admin(BatteryOS *bos); 

    /** handle battery message from file descriptor fd */
    static int handle_battery(BatteryOS *bos, const std::string &battery_name); 

    void shutdown() {
        LOG() << "shutting down"; 
        this->dir.quit(); 
        for (auto &it : this->battery_ifds) {
            ::close(it.second); 
        }
        ::close(this->admin_fifo_ifd); 
        // close dynamic libraries 
        for (auto &it : this->loaded_dynamic_libs) {
            dlclose(it.second); 
        }
        // unlink the fds 
        std::string admin_fifo_path = this->dirpath + "admin" + "_input"; 
        unlink(admin_fifo_path.c_str()); 
        std::string admin_fifo_path2 = this->dirpath + "admin" + "_output"; 
        unlink(admin_fifo_path2.c_str()); 
        for (auto &it : this->battery_ifds) {
            std::string fifo_path = this->dirpath + it.first + "_input"; 
            unlink(fifo_path.c_str()); 
            std::string fifo_path2 = this->dirpath + it.first + "_output"; 
            unlink(fifo_path2.c_str()); 
        }
        // unlink the dir 
        rmdir(this->dirpath.c_str()); 
        // don't quit twice on destructor 
        this->should_quit = false; 
        this->quitted = true; 
        LOG() << "bye~"; 
    }
};




#endif // ! BOS_HPP 



































