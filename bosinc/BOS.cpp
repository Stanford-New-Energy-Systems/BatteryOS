#include "BOS.hpp"
#include "ProtobufMsg.hpp"
#include <sys/stat.h>
#include <dirent.h>
#include <poll.h>
#ifndef BATTERY_FACTORY_NAME_CHECK
#define BATTERY_FACTORY_NAME_CHECK(factory, name)\
    do {\
        if ((factory)->dir.name_exists(name)) {\
            WARNING() << "Battery name " << (name) << " already exists!";\
            return NULL;\
        }\
    } while (0)
#endif  // ! BATTERY_FACTORY_NAME_CHECK

std::map<std::string, void*> BatteryDirectoryManager::loaded_dynamic_libs; 

Battery *BatteryDirectoryManager::make_null(
    const std::string &name,
    int64_t voltage_mV,
    int64_t max_staleness_ms
) {
    BATTERY_FACTORY_NAME_CHECK(this, name);
    std::unique_ptr<Battery> null_battery(
        new NullBattery(
            name, 
            voltage_mV, 
            std::chrono::milliseconds(max_staleness_ms)
        )
    );
    return this->dir.add_battery(std::move(null_battery));
}

Battery *BatteryDirectoryManager::make_pseudo(
    const std::string &name, 
    BatteryStatus status,
    int64_t max_staleness_ms
) {
    BATTERY_FACTORY_NAME_CHECK(this, name);
    std::unique_ptr<Battery> pseudo_battery(
        new PseudoBattery(
            name, 
            status, 
            std::chrono::milliseconds(max_staleness_ms)
        )
    );
    return this->dir.add_battery(std::move(pseudo_battery));
}

Battery *BatteryDirectoryManager::make_JBDBMS(
    const std::string &name, 
    const std::string &device_address, 
    const std::string &rd6006_address, 
    int64_t max_staleness_ms
) {
    BATTERY_FACTORY_NAME_CHECK(this, name);
    std::unique_ptr<Connection> pconn(
        new UARTConnection(device_address)
    );
    std::unique_ptr<CurrentRegulator> pregulator(
        new RD6006PowerSupply(rd6006_address)
    );
    std::unique_ptr<Battery> jbd(
        new JBDBMS(
            name, 
            std::move(pconn), 
            std::move(pregulator), 
            std::chrono::milliseconds(max_staleness_ms)
        )
    );
    return this->dir.add_battery(std::move(jbd));
}

Battery *BatteryDirectoryManager::make_networked_battery(
    const std::string &name, 
    const std::string &remote_name, 
    const std::string &address, 
    int port, 
    int64_t max_staleness_ms
) {
    BATTERY_FACTORY_NAME_CHECK(this, name);

    std::unique_ptr<Connection> pconn(
        new TCPConnection(
            address, 
            port
        )
    );
    if (!(pconn->connect())) {
        WARNING() << "TCP connection failed!";
        return nullptr;
    }

    std::unique_ptr<Battery> network(
        new NetworkBattery(
            name, 
            remote_name, 
            std::move(pconn), 
            std::chrono::milliseconds(max_staleness_ms)
        )
    );
    return this->dir.add_battery(std::move(network));
}

Battery *BatteryDirectoryManager::make_aggregator(
    const std::string &name, 
    int64_t voltage_mV, 
    int64_t voltage_tolerance_mV, 
    const std::vector<std::string> &src_names, 
    int64_t max_staleness_ms
) {
    BATTERY_FACTORY_NAME_CHECK(this, name);
    Battery *bat;
    BatteryStatus pstatus;
    int64_t v;
    bool failed = false;
    for (const std::string &src : src_names) {
        bat = this->dir.get_battery(src);
        if (!bat) {
            WARNING() << "Battery " << name << "not found!";
            failed = true;
            continue;
        }
        pstatus = bat->get_status();
        v = pstatus.voltage_mV;
        if (!(voltage_mV - voltage_tolerance_mV <= v && v <= voltage_mV + voltage_tolerance_mV)) {
            WARNING() << "Battery " << name << " is out of voltage tolerance!";
            failed = true;
        }
        if (pstatus.current_mA != 0) {
            WARNING() << "Battery in use!";
            failed = true;
        }
    }
    if (failed) {
        return nullptr;
    }
    std::unique_ptr<Battery> aggregator(
        new AggregatorBattery(
            name, 
            voltage_mV, 
            voltage_tolerance_mV, 
            src_names, 
            this->dir, 
            std::chrono::milliseconds(max_staleness_ms)));
    Battery *agg = this->dir.add_battery(std::move(aggregator));
    for (const std::string &src : src_names) {
        this->dir.add_edge(src, name);
    }
    return agg;
}

std::vector<Battery*> BatteryDirectoryManager::make_splitter(
    const std::string &name, 
    const std::string &src_name, 
    const std::vector<std::string> &child_names, 
    const std::vector<Scale> &child_scales, 
    const std::vector<int64_t> &child_max_stalenesses_ms, 
    int policy_type,
    int64_t max_staleness_ms
) {
    if (this->dir.name_exists(name)) {
        WARNING() << "Battery name " << (name) << " already exists!";
        return {};
    }
    if (!this->dir.name_exists(src_name)) {
        WARNING() << "source battery " << src_name << " does not exist";
        return {};
    }

    if (!(child_names.size() == child_scales.size() && child_names.size() == child_max_stalenesses_ms.size())) {
        WARNING() << "array size mismatch";
        return {}; 
    }
    bool failed = false;
    for (const std::string &cn : child_names) {
        if (this->dir.name_exists(cn)) {
            WARNING() << "child name " << cn << " already exists!";
            failed = true;
        }
    }
    if (failed) {
        return {};
    }

    Scale total_sum(0.0);
    for (size_t i = 0; i < child_names.size(); ++i) {
        total_sum = total_sum + child_scales[i];
        if (total_sum.is_zero()) {
            WARNING() << "sum of child scales is not within [0, 1]";
            return {};
        }
    }
    
    if (!total_sum.is_one()) {
        WARNING() << "sum of child scales is not 1";
        return {};
    }

    std::vector<Battery*> children;
    for (size_t i = 0; i < child_names.size(); ++i) {
        std::unique_ptr<Battery> child(
            new SplittedBattery(
                child_names[i], 
                this->dir, 
                std::chrono::milliseconds(child_max_stalenesses_ms[i])
            )
        );
        children.push_back(this->dir.add_battery(std::move(child)));
    }
    std::unique_ptr<Battery> policy(
        new BALSplitter(
            name, 
            src_name, 
            this->dir, 
            child_names, 
            child_scales, 
            BALSplitterType(policy_type), 
            std::chrono::milliseconds(max_staleness_ms)
        )
    );
    BALSplitter *pptr = dynamic_cast<BALSplitter*>(policy.get());
    this->dir.add_battery(std::move(policy));
    this->dir.add_edge(src_name, name);
    for (const std::string &cn : child_names) {
        this->dir.add_edge(name, cn);
        SplittedBattery *cp = dynamic_cast<SplittedBattery*>(this->dir.get_battery(cn));
        cp->attach_to_policy(name);
    }
    pptr->start_background_refresh();
    
    return children;
}


Battery *BatteryDirectoryManager::make_dynamic(
    const std::string &name, 
    const std::string &dynamic_lib_path, 
    int64_t max_staleness_ms, 
    const char *init_argument, 
    const std::string &init_func_name, 
    const std::string &destruct_func_name, 
    const std::string &get_status_func_name, 
    const std::string &set_current_func_name, 
    const std::string &get_delay_func_name
) {
    using init_func_t = typename DynamicBattery::init_func_t;
    using destruct_func_t = typename DynamicBattery::destruct_func_t; 
    using get_status_func_t = typename DynamicBattery::get_status_func_t; 
    using set_current_func_t = typename DynamicBattery::set_current_func_t; 
    using get_delay_func_t = typename DynamicBattery::get_delay_func_t; 

    void *lib_handle = nullptr; 
    if (loaded_dynamic_libs.find(dynamic_lib_path) != loaded_dynamic_libs.end()) {
        lib_handle = loaded_dynamic_libs[dynamic_lib_path]; 
    } else {
        lib_handle = dlopen(dynamic_lib_path.c_str(), RTLD_LAZY); 
        if (!lib_handle) {
            WARNING() << "dynamic_lib not opened! Error: " << dlerror();
            return nullptr;
        } 
    } 
    bool failed = false;
    init_func_t init_func = (init_func_t)dlsym(lib_handle, init_func_name.c_str());
    if (!init_func) { 
        WARNING() << "init_func not found! init_func = " << init_func_name << " Error: " << dlerror(); 
        failed = true; 
    }
    destruct_func_t destruct_func = (destruct_func_t)dlsym(lib_handle, destruct_func_name.c_str());
    if (!destruct_func) { 
        WARNING() << "destruct_func not found! destruct_func = " << destruct_func_name << " Error: " << dlerror(); 
        failed = true; 
    }
    get_status_func_t get_status_func = (get_status_func_t)dlsym(lib_handle, get_status_func_name.c_str());
    if (!get_status_func) { 
        WARNING() << "get_status_func not found! get_status_func = " << get_status_func_name << " Error: " << dlerror(); 
        failed = true; 
    }
    set_current_func_t set_current_func = (set_current_func_t)dlsym(lib_handle, set_current_func_name.c_str());
    if (!set_current_func) { 
        WARNING() << "set_current_func not found! set_current_func = " << set_current_func_name << " Error: " << dlerror(); 
        failed = true; 
    }
    get_delay_func_t get_delay_func = (get_delay_func_t)dlsym(lib_handle, get_delay_func_name.c_str());
    if (!get_delay_func) {
        get_delay_func = &DynamicBattery::no_delay;
    }

    if (failed) {
        WARNING() << "dynamic_lib failed to find required symbols!";
        init_func = nullptr;
        destruct_func = nullptr;
        get_status_func = nullptr;
        set_current_func = nullptr;
        dlclose(lib_handle); 
        return nullptr; 
    }
    loaded_dynamic_libs[dynamic_lib_path] = lib_handle; 

    void *init_result = init_func(init_argument); 
    if (!init_result) {
        WARNING() << "init_func " << init_func_name << " failed to initialize with argument " << init_argument;
        return nullptr; 
    }
    
    std::unique_ptr<DynamicBattery> dynamic_battery(
        new DynamicBattery(
            name, 
            std::chrono::milliseconds(max_staleness_ms), 
            dynamic_lib_path, 
            init_func, 
            destruct_func, 
            get_status_func,  
            set_current_func, 
            get_delay_func, 
            init_result, 
            init_argument
        )
    );
    return this->dir.add_battery(std::move(dynamic_battery)); 
}


int BatteryDirectoryManager::remove_battery(const std::string &name) {
    Battery *bat = this->dir.get_battery(name);
    if (!bat) {
        WARNING() << "no battery named " << name << " is found";
        return 0;
    }
    if (bat->get_battery_type() == BatteryType::Splitted) {
        WARNING() << "battery " << name << " is a partitioned battery, please remove the partitioner";
        return 0;
    }
    return this->dir.remove_battery(name);
}

void BatteryOS::simple_remote_connection_server(int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        ERROR() << "failed to create socket";
    }
    sockaddr_in sockaddr;
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_addr.s_addr = INADDR_ANY;
    sockaddr.sin_port = htons(port);
    if (bind(sockfd, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) < 0) {
        ERROR() << "Failed to bind to port, errno: " << errno;
    }
    if (listen(sockfd, 10) < 0) {
        ERROR() << "Failed to listen on socket. errno: " << errno;
    }
    
    size_t addrlen = sizeof(sockaddr);
    int connection = accept(sockfd, (struct sockaddr*)&sockaddr, (socklen_t*)&addrlen);

    if (connection < 0) {
        ERROR() << "Failed to grab connection. errno: " << errno;
    }

    // std::thread t([=]()
    {
        TCPConnection conn("", 0);

        conn.port = port;
        conn.socket_fd = connection;

        while (true) {
            std::vector<uint8_t> len_buf = conn.read(4);
            if (len_buf.size() != 4) {
                WARNING() << "it seems like the data length is < 4! data_length = " << len_buf.size();
            }
            uint32_t data_len = deserialize_int<uint32_t>(len_buf.data());
            if (data_len == 0) {
                ::close(connection);
                while (true);
            }

            std::vector<uint8_t> data_buf = conn.read(data_len);

            RPCRequestHeader header;

            size_t header_bytes = RPCRequestHeader_deserialize(&header, data_buf.data(), data_buf.size());
            if (header_bytes != sizeof(RPCRequestHeader)) {
                WARNING() << "header_bytes received does not equal to the size of the header!!!";
            }

            std::string name;

            for (size_t i = header_bytes; i < data_buf.size()-1; ++i) {
                name.push_back((char)data_buf[i]);
            }

            Battery *bat = dir.get_battery(name);
            if (!bat) {
                WARNING() << "battery " << name << " not found!";
            }

            switch (header.func) {
            case int(RPCFunctionID::REFRESH): 
            case int(RPCFunctionID::GET_STATUS): { 
                BatteryStatus status = bat->get_status();
                LOG() << "remote get_status: " << status;
                std::vector<uint8_t> buf(sizeof(uint32_t) + sizeof(BatteryStatus));
                serialize_int<uint32_t>(sizeof(BatteryStatus), buf.data(), buf.size());
                BatteryStatus_serialize(&status, buf.data() + sizeof(uint32_t), buf.size());
                conn.write(buf);
            } break;
            case int(RPCFunctionID::SET_CURRENT): {
                int64_t current_mA = header.current_mA;
                timepoint_t when_to_set = c_time_to_timepoint(header.when_to_set);
                timepoint_t until_when = c_time_to_timepoint(header.until_when);
                LOG() << "remote schedule_set_current: " << current_mA;
                uint32_t r = bat->schedule_set_current(current_mA, false, when_to_set, until_when);
                std::vector<uint8_t> buf(sizeof(uint32_t) + sizeof(uint32_t));
                serialize_int(sizeof(uint32_t), buf.data(), buf.size());
                serialize_int(r, buf.data() + sizeof(uint32_t), buf.size());
                conn.write(buf);
            } break;
            default:
                WARNING() << "Unknown request";
                break;
            }
        }
        return;
    }
    // );
    // t.join();
}


int BatteryOS::connection_handler(Connection *conn) {
    std::vector<uint8_t> len_buf = conn->read(4);
    if (len_buf.size() != 4) {
        WARNING() << "it seems like the data length is < 4! data_length = " << len_buf.size();
        conn->flush();
        return -5;
    }
    uint32_t data_len = deserialize_int<uint32_t>(len_buf.data());
    if (data_len == 0) {
        WARNING() << "received wrong data, failed to deserialize to uint32_t";
        conn->flush();
        return -2;
    }

    std::vector<uint8_t> data_buf = conn->read(data_len);

    RPCRequestHeader header;

    size_t header_bytes = RPCRequestHeader_deserialize(&header, data_buf.data(), data_buf.size());
    if (header_bytes != sizeof(RPCRequestHeader)) {
        WARNING() << "header_bytes received does not equal to the size of the header!!!";
        conn->flush();
        return -3;
    }

    std::string name;

    for (size_t i = header_bytes; i < data_buf.size()-1; ++i) {
        name.push_back((char)data_buf[i]);
    }

    Battery *bat = dir.get_battery(name);
    if (!bat) {
        WARNING() << "battery " << name << " not found!";
        conn->flush();
        return -1;
    }

    switch (header.func) {
        case int(RPCFunctionID::REFRESH): 
        case int(RPCFunctionID::GET_STATUS): { 
            BatteryStatus status = bat->get_status();
            LOG() << "remote get_status: " << status;
            std::vector<uint8_t> buf(sizeof(uint32_t) + sizeof(BatteryStatus));
            serialize_int<uint32_t>(sizeof(BatteryStatus), buf.data(), buf.size());
            BatteryStatus_serialize(&status, buf.data() + sizeof(uint32_t), buf.size());
            conn->write(buf);
        } break;
        case int(RPCFunctionID::SET_CURRENT): {
            int64_t current_mA = header.current_mA;
            timepoint_t when_to_set = c_time_to_timepoint(header.when_to_set);
            timepoint_t until_when = c_time_to_timepoint(header.until_when);
            LOG() << "remote schedule_set_current: " << current_mA;
            uint32_t r = bat->schedule_set_current(current_mA, false, when_to_set, until_when);
            std::vector<uint8_t> buf(sizeof(uint32_t) + sizeof(uint32_t));
            serialize_int(sizeof(uint32_t), buf.data(), buf.size());
            serialize_int(r, buf.data() + sizeof(uint32_t), buf.size());
            conn->write(buf);
        } break;
        default:
            WARNING() << "Unknown request";
            conn->flush();
            return -4;
    }
    return 0;
}

int BatteryOS::ensure_dir(const std::string &dpath, mode_t permission) {
    DIR *d = opendir(dpath.c_str()); 
    if (!d) {
        if (mkdir(dpath.c_str(), permission) < 0) {
            WARNING() << "failed to mkdir";
            return -1;
        }
    } else {
        closedir(d); 
    }
    return 0; 
}

int BatteryOS::admin_fifo_init(mode_t permission, mode_t dir_permission) {
    if (ensure_dir(dirpath, dir_permission) < 0) {
        WARNING() << "Error creating the dir"; 
        return -3; 
    } 
    std::string admin_fifo_name = dirpath + "admin";
    if (mkfifo(admin_fifo_name.c_str(), permission) < 0) {
        WARNING() << "failed to make admin fifo"; 
        return -1;
    }
    int fd = open(admin_fifo_name.c_str(), O_RDWR); 
    if (fd < 0) {
        WARNING() << "failed to open admin fifo!"; 
        return -2; 
    }
    this->admin_fifo_fd = fd; 
    return 0; 
}  

int BatteryOS::battery_fifo_init(mode_t permission, mode_t dir_permission) {
    std::vector<std::string> name_list = dir.get_name_list();
    if (ensure_dir(dirpath, dir_permission) < 0) {
        WARNING() << "Error creating the dir"; 
        return -3;
    } 
    std::string temp;
    for (size_t i = 0; i < name_list.size(); ++i) {
        temp = dirpath + name_list[i];
        if (mkfifo(temp.c_str(), permission) < 0) {
            WARNING() << "mkfifo failed for battery " << temp;
            return -1;
        }
        int fd = open(temp.c_str(), O_RDWR); 
        if (fd < 0) {
            WARNING() << "failed to open battery fifo for battery " << name_list[i]; 
            return -2; 
        }
        this->battery_fds[name_list[i]] = fd; 
        this->fd_to_battery_name[fd] = name_list[i];  
    }
    return 0;
}

int BatteryOS::single_battery_fifo_init(const std::string &battery_name, mode_t permission) {
    std::vector<std::string> name_list = dir.get_name_list();
    std::string fifo_path = this->dirpath + battery_name; 
    if (mkfifo(fifo_path.c_str(), permission) < 0) {
        WARNING() << "mkfifo failed for battery " << battery_name;
        return -1;
    }
    int fd = open(fifo_path.c_str(), O_RDWR); 
    if (fd < 0) {
        WARNING() << "failed to open battery fifo for battery " << battery_name; 
        return -2;
    }
    this->battery_fds[battery_name] = fd; 
    this->fd_to_battery_name[fd] = battery_name;  
    return 0; 
}

int BatteryOS::poll_fifos() {
    pollfd *fds = new pollfd[this->battery_fds.size() + 1]; 
    if (!fds) {
        WARNING() << "failed to allocate memory?"; 
        return -1; 
    }
    fds[0].fd = this->admin_fifo_fd; 
    fds[0].events = POLLIN | POLLPRI; 
    fds[0].revents = 0; 

    size_t i = 1;
    for (auto &it : this->battery_fds) {
        fds[i].fd = it.second;
        fds[i].events = POLLIN | POLLPRI; 
        fds[i].revents = 0; 
        i += 1; 
    }
    int retval = 0;
    while (1) {
        retval = poll(fds, this->battery_fds.size()+1, -1); 
        if (retval < 0) {
            WARNING() << "Poll failed!!!"; 
            delete [] fds; 
            return -1; 
        }
        for (size_t j = 0; j < this->battery_fds.size()+1; ++j) {
            if ((fds[j].revents & POLLIN) == POLLIN) {
                if (j == 0) {
                    if (this->handle_admin(fds[j].fd) < 0) {
                        WARNING() << "failed to handle admin message";
                    } 
                } else {
                    if (this->handle_battery(fds[j].fd) < 0) {
                        WARNING() << "failed to handle battery message";
                    }
                }
                fds[j].revents = 0; 
            }
        }
    }
    delete [] fds; 
    return 0; 
    // fds[i].fd = open(temp.c_str(), O_RDWR | O_NONBLOCK);
    // fds[i].events = POLLIN | POLLPRI;
    // fds[i].revents = 0;
    // int retval;
    // while (1) {
    //     retval = poll(fds, name_list.size(), -1);
    //     if (retval < 0) {
    //         WARNING() << "Poll failed!";
    //         delete [] fds;
    //         return -1;
    //     }
    //     for (size_t i = 0; i < name_list.size(); ++i) {
    //         if (fds[i].revents & POLLIN == POLLIN) {
    //             // handle data 
    //             FIFOConnection conn("", 0777, false);
    //             conn.connected = true;
    //             conn.fd = fds[i].fd;
    //             if (connection_handler(&conn) < 0) {
    //                 WARNING() << "something wrong?";
    //             }
    //             conn.fd = 0;
    //             conn.connected = false;
    //         }
    //     }
    // }
    // delete [] fds;
}