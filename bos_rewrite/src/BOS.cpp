#include "BOS.hpp"

/**********************
Constructor/Destructor
***********************/

BOS::~BOS() {
    delete[] this->fds;
    if (!this->hasQuit)
        this->shutdown();
    dlclose(this->library);
}

BOS::BOS() {
    this->hasQuit          = true;
    this->quitPoll         = false;
    this->adminFifoFD      = -1;
    this->adminConnection      = nullptr;
    this->fds = new pollfd[1028];
    this->directoryManager = std::make_unique<BatteryDirectoryManager>();

    this->library = dlopen(DYLIB_PATH("../tests/libbatterydrivers"), RTLD_LAZY);
    if (!this->library)
        ERROR() << "could not open dynamic library!" << std::endl;
} // use only for socket mode

BOS::BOS(const std::string &directoryPath, mode_t permission) : BOS() {
    this->createDirectory(directoryPath, permission);
} // use for both FIFO and socket mode 

/*****************
Private Functions
******************/

void BOS::pollFDs() {
    while(!this->quitPoll) { // think of best way to terminate this loop 
        netServicer.poll();
    } 
    return;
}

void BOS::acceptAdminConnection(Stream* stream) {
    this->adminConnection = std::make_shared<BatteryConnection>(std::unique_ptr<Stream>(stream));
    this->adminConnection->messageReadyHandler = [this](BatteryConnection* connection) {
        std::cout << "ADMIN COMMAND!" << std::endl;
        this->handleAdminCommand(*connection);
    };
    netServicer.add(this->adminConnection);
}

void BOS::acceptBatteryConnection(const std::string& batteryName, std::shared_ptr<BatteryConnection> connection) {
    connection->messageReadyHandler = [this, batteryName](BatteryConnection* connection) {
        this->handleBatteryCommand(batteryName, *connection);
    };
    netServicer.add(connection);
    this->connections.push_back(connection);
}

void BOS::handleBatteryCommand(const std::string& batteryName, BatteryConnection& connection) {
    bosproto::BatteryCommand command;
    int success = connection.read(command);

    if (!success) {
        WARNING() << "could not parse BatteryCommand" << std::endl;
        return;
    } 

    std::cout << "COMMAND: " << command.DebugString() << std::endl;

    switch(command.command()) {
        case bosproto::Command::Get_Status:
            this->getStatus(batteryName, connection);
            break;
        case bosproto::Command::Set_Status:
            this->setStatus(command, batteryName, connection);
            break;
        case bosproto::Command::Schedule_Set_Current:
            this->scheduleSetCurrent(command, batteryName, connection);
            break;
        //case bosproto::Command::Remove_Battery:
        //    this->removeBattery(fd);
        //    break;
        default:
            WARNING() << "need to select battery command option" << std::endl;
            break;
    }

    return;
}

void BOS::getStatus(const std::string& batteryName, BatteryConnection& connection) {
    std::cout << "GET STATUS: " << batteryName << std::endl;

    bosproto::BatteryStatusResponse response;
    bosproto::BatteryStatus* s = response.mutable_status();

    std::shared_ptr<Battery> bat = this->directoryManager->getBattery(batteryName);
    BatteryStatus status = bat->getStatus();
    status.toProto(*response.mutable_status());

    response.set_return_code(0);    
    std::cout << "STATUS: " << response.DebugString() << std::endl;
    connection.write(response);
}

void BOS::setStatus(const bosproto::BatteryCommand& command, const std::string& batteryName, BatteryConnection& connection) {
    bosproto::SetStatusResponse response;
    if (!command.has_status()) {
        response.set_return_code(-1);
        response.set_reason("status needs to be set!");

        connection.write(response);
        return;
    }

    BatteryStatus status(command.status());

    std::shared_ptr<Battery> battery = this->directoryManager->getBattery(batteryName);

    if (battery == nullptr) {
        response.set_return_code(-1);
        response.set_reason("battery does not exist in directory!");
    } else {
        battery->setBatteryStatus(status);
        response.set_return_code(0);
        response.set_reason("successfully set status!");
    }

    connection.write(response);
}

void BOS::scheduleSetCurrent(const bosproto::BatteryCommand& command, const std::string& batteryName, BatteryConnection& connection) {
    bosproto::ScheduleSetCurrentResponse response;
    if (!command.has_schedule_set_current()) {
        response.set_return_code(-1);
        response.set_failure_message("must set current, start time, and end time");
       
        connection.write(response);

        return;
    } 

    bosproto::ScheduleSetCurrent params = command.schedule_set_current();
    double current_mA  = params.current_ma();
    uint64_t startTime = params.starttime();
    uint64_t endTime   = params.endtime(); 

    std::shared_ptr<Battery> bat = this->directoryManager->getBattery(batteryName);
    bool success = bat->schedule_set_current(current_mA, startTime, endTime);

    if (!success) {
        response.set_return_code(-1);
        response.set_failure_message("failure setting current for " + batteryName);
    } else {
        response.set_return_code(0);
        response.set_success_message("successfully set current for " + batteryName);
    }

    if (!connection.write(response)) {
        WARNING() << "Unable to serialize response to " << batteryName << std::endl;
    }
}

void BOS::removeBattery(int fd) {
    //int output_fd;
    //bosproto::RemoveBatteryResponse response;

    //std::string name = this->battery_names[fd].first;

    //if (this->battery_names[fd].second) {
    //    std::string outputFile = this->fileNames[fd].second;
    //    output_fd = open(outputFile.c_str(), O_WRONLY);
    //} else
    //    output_fd = fd;

    //if (output_fd == -1) {
    //    WARNING() << "Unable to open " << name << "'s output FIFO" << std::endl;
    //    return;
    //}
    //
    //bool success = this->directoryManager->removeBattery(name);

    //if (!success) {
    //    response.set_return_code(-1);
    //    response.set_failure_message("Unable to remove battery");
    //} else {
    //    response.set_return_code(0);
    //    response.set_success_message("Successfully removed battery");

    //    close(fd);
    //    this->battery_names.erase(fd);
    //    this->fileNames.erase(fd);  
    //}

    //if (!response.SerializeToFileDescriptor(output_fd))
    //    WARNING() << "unable to write remove battery response to output FIFO" << std::endl;

    //if (this->battery_names[fd].second)
    //    close(output_fd);

    //return;
}

void BOS::handleAdminCommand(BatteryConnection& connection) {
    bosproto::Admin_Command command;

    int success = connection.read(command);
    if (!success) {
        WARNING() << "could not parse Admin_Command" << std::endl;
        return;
    }

    switch(command.command_options()) {
        case bosproto::Command_Options::Create_Physical:
            this->createPhysicalBattery(command, connection); 
            break;
        case bosproto::Command_Options::Create_Aggregate:
            this->createAggregateBattery(command, connection);
            break;
        case bosproto::Command_Options::Create_Partition:
            this->createPartitionBattery(command, connection);
            break;
        case bosproto::Command_Options::Create_Dynamic:
            this->createDynamicBattery(command, connection);
            break;
        case bosproto::Command_Options::Shutdown:
            this->shutdown();
            break;
        default:
            WARNING() << "invalid Command_Options" << std::endl;
            bosproto::AdminResponse response;
            response.set_return_code(-1);
            response.set_failure_message("Invalid admin command!");
            connection.write(response);
            break;
    }
    return;
}

void BOS::createBatteryFifos(const std::string& batteryName) {
    std::cout << "Creating battery fifos" << std::endl;
    std::string path = this->directoryPath + batteryName;

    std::shared_ptr<FifoAcceptor> acceptor = std::make_shared<FifoAcceptor>(path, batteryName, [this](FifoPipe* pipe) {
        std::shared_ptr<BatteryConnection> connection = std::make_shared<BatteryConnection>(std::unique_ptr<Stream>(pipe));
        this->acceptBatteryConnection(pipe->name, connection);
    }, true);
    this->fifos.push_back(acceptor);
    this->netServicer.add(this->fifos[this->fifos.size() - 1]);
}

void BOS::createPhysicalBattery(const bosproto::Admin_Command& command, BatteryConnection& connection) {
    bosproto::AdminResponse response;

    std::cout << "GOT COMMAND: " << command.DebugString() << std::endl;
    if (!command.has_physical_battery()) {
        response.set_return_code(-1);
        response.set_failure_message("Physical_Battery parameters not set!");
        
        if (connection.write(response)) {
            WARNING() << "unable to write failure message to file descriptor" << std::endl;
        }

        return;
    }

    paramsPhysical b = parsePhysicalBattery(command.physical_battery());
    std::shared_ptr<Battery> bat = this->directoryManager->createPhysicalBattery(b.name, b.staleness, b.refresh);

    if (!bat) {
        response.set_return_code(-1);
        response.set_failure_message("battery name: " +  b.name + " already exists");
        connection.write(response);

        return;
    }

    response.set_return_code(0);
    response.set_success_message("successfully created battery: " + b.name);

    if (this->mode == BOSMode::Fifo) {
        createBatteryFifos(b.name);
    }

    if (!connection.write(response)) {
        WARNING() << "unable to write message to file descriptor" << std::endl;
    }

    return;
}

void BOS::createAggregateBattery(const bosproto::Admin_Command& command, BatteryConnection& connection) {
    //int output_fd;

    //if (FIFO) {
    //    std::string adminOutputFile = this->fileNames[this->adminBFifoFD].second;
    //    output_fd = open(adminOutputFile.c_str(), O_WRONLY);
    //} else
    //    output_fd = fd;

    bosproto::AdminResponse response;
    if (!command.has_aggregate_battery()) {
        response.set_return_code(-1);
        response.set_failure_message("Aggregate_Battery parameters not set!");
        
        if (connection.write(response)) {
            WARNING() << "unable to write failure message to file descriptor" << std::endl;
        }

        //if (FIFO)
        //    close(output_fd);

        return;
    }

    paramsAggregate b = parseAggregateBattery(command.aggregate_battery());
    std::cout << "Creating aggregate battery " << b.name << std::endl;
    std::shared_ptr<Battery> bat = this->directoryManager->createAggregateBattery(b.name, b.parents, b.staleness, b.refresh);     

    if (!bat) {
        response.set_return_code(-1);
        response.set_failure_message("error creating aggregate battery!");
        connection.write(response);

        return;
    }

    if (this->mode == BOSMode::Fifo) {
        createBatteryFifos(b.name);
    }

    response.set_return_code(0);
    response.set_success_message("successfully created battery: " + b.name);

    if (!connection.write(response)) {
        WARNING() << "unable to write message to file descriptor" << std::endl;
    }
}

void BOS::createPartitionBattery(const bosproto::Admin_Command& command, BatteryConnection& connection) {
    bosproto::AdminResponse response;
    if (!command.has_partition_battery()) {
        response.set_return_code(-1);
        response.set_failure_message("Partition_Battery parameters not set!");
        
        if (connection.write(response)) {
            WARNING() << "unable to write failure message to file descriptor" << std::endl;
        }

        return;
    }

    paramsPartition b = parsePartitionBattery(command.partition_battery());

    std::vector<std::shared_ptr<Battery>> batteries;
    if (b.refreshModes.size() == 0 && b.stalenesses.size() == 0) {
        batteries = this->directoryManager->createPartitionBattery(b.source, b.policy, b.child_names, b.proportions); 
    } else if (b.refreshModes.size() == 0 || b.stalenesses.size() == 0) {
        response.set_return_code(-1);
        response.set_failure_message("refresh modes and stalenesses both need to be included or excluded: cannot choose one");

        if (!connection.write(response)) { 
            WARNING() << "unable to write failure message to file descriptor" << std::endl;
        }

        return;
    } else {
        batteries = this->directoryManager->createPartitionBattery(b.source, b.policy, b.child_names, b.proportions, b.stalenesses, b.refreshModes); 
    }

    if (batteries.size() == 0) {
        response.set_return_code(-1);
        response.set_failure_message("error creating partition batteries!");
        connection.write(response);
        return;
    }

    if (this->mode == BOSMode::Fifo) {
        for (auto& name : b.child_names) {
            createBatteryFifos(name);
        }
    }

    response.set_return_code(0);
    response.set_success_message("successfully created batteries!");

    if (!connection.write(response)) {
        WARNING() << "unable to write message to file descriptor" << std::endl;
    }
}

void BOS::createDynamicBattery(const bosproto::Admin_Command& command, BatteryConnection& connection) {
    bosproto::AdminResponse response;
    if (!command.has_dynamic_battery()) {
        WARNING() << "entering this branch :(" << std::endl;
        response.set_return_code(-1);
        response.set_failure_message("Dynamic_Battery parameters not set!");
        
        if (connection.write(response)) {
            WARNING() << "unable to write failure message to file descriptor" << std::endl;
        }

        return;
    }

    paramsDynamic b = parseDynamicBattery(command.dynamic_battery());

    void* initArgs = (void*)b.initArgs;
    void* destructor     = dlsym(this->library, b.destructor.c_str());
    void* constructor    = dlsym(this->library, b.constructor.c_str());
    void* refreshFunc    = dlsym(this->library, b.refreshFunc.c_str());
    void* setCurrentFunc = dlsym(this->library, b.setCurrentFunc.c_str());


    if (constructor == NULL || destructor == NULL ||
        refreshFunc == NULL || setCurrentFunc == NULL) {
        response.set_return_code(-1);
        response.set_failure_message("function names not found in dynamic library file");

        if (!connection.write(response)) {
            WARNING() << "unable to write message to file descriptor" << std::endl;
        }

        return;
    }

    std::shared_ptr<Battery> bat = this->directoryManager->createDynamicBattery(initArgs, destructor, constructor,
                                                                                refreshFunc, setCurrentFunc, b.name,
                                                                                b.staleness, b.refresh);
     
    LOG() << "made it here!" << std::endl;

    delete[] b.initArgs;

    if (!bat) {
        response.set_return_code(-1);
        response.set_failure_message("battery name: " +  b.name + " already exists");
        connection.write(response);
        return;
    }

    if (this->mode == BOSMode::Fifo) {
        createBatteryFifos(b.name);
    }

    response.set_return_code(0);
    response.set_success_message("successfully created battery: " + b.name);

    if (!connection.write(response)) {
        WARNING() << "unable to write message to file descriptor" << std::endl;
    }
}

void BOS::createDirectory(const std::string &directoryPath, mode_t permission) {
    if (mkdir(directoryPath.c_str(), permission) == -1)
        WARNING() << directoryPath << " already exists" << std::endl;
    this->directoryPath = directoryPath + "/";
    return;
}

/****************
Public Functions
*****************/

void BOS::startFifos(mode_t adminPermission) {
    this->hasQuit = false;
    this->mode = BOSMode::Fifo;

    std::string path  = this->directoryPath + "admin";
    
    this->adminListener = std::make_shared<FifoAcceptor>(path, "admin", [this](FifoPipe* pipe) {
        this->acceptAdminConnection(pipe);  
    }, true);
    std::cout << "CREATED!" << std::endl;
    netServicer.add(this->adminListener);

    this->pollFDs();
}

void BOS::startSockets(int adminPort, int batteryPort) {
    this->hasQuit = false;
    this->mode = BOSMode::Network;

    this->adminListener = std::make_shared<Acceptor>(INADDR_ANY, adminPort, 1, [this](Socket* socket) {
        this->acceptAdminConnection(socket);
    });
    netServicer.add(this->adminListener);

    this->batteryListener = std::make_shared<Acceptor>(INADDR_ANY, batteryPort, 1024, [this](Socket* socket) {
        bosproto::BatteryConnect command;
        std::shared_ptr<BatteryConnection> connection = std::make_shared<BatteryConnection>(std::unique_ptr<Stream>(socket));
 
        int bytes_read = connection->read(command);
        std::cout << "Battery connect: " << command.batteryname() << std::endl;

        bosproto::ConnectResponse response;
        if (bytes_read == -1) {
            WARNING() << "Unable to read batteryName from clientSocket!" << std::endl;
            response.set_status_code(bosproto::ConnectStatusCode::GenericError);
        } else if (this->directoryManager->getBattery(command.batteryname()) == nullptr) {
            WARNING() << "Get battery nullptr: " << command.batteryname() << std::endl;
            response.set_status_code(bosproto::ConnectStatusCode::DoesNotExist);
        } else {
            this->acceptBatteryConnection(command.batteryname(), connection);
            response.set_status_code(bosproto::ConnectStatusCode::Success);
        }

        connection->write(response);
    });
    netServicer.add(this->batteryListener);

    this->pollFDs();
}

void BOS::shutdown() {
    LOG() << "SHUTTING DOWN!" << std::endl;

    this->hasQuit  = true;
    this->quitPoll = true;

    for (const auto &f: fileNames) {
        LOG() << "Closing " << f.second.first << std::endl;

        close(f.first);

        LOG() << "Deleting " << f.second.first  << std::endl; 
        LOG() << "Deleting " << f.second.second << std::endl; 

        unlink(f.second.first.c_str());
        unlink(f.second.second.c_str());

        LOG() << "Removing " << this->directoryPath << std::endl;

        rmdir(this->directoryPath.c_str());
    }

    //for (const auto &f: battery_names) {
    //    if (f.second.second)
    //        close(f.first);
    //}

    //if (this->adminSocketFD != -1)
    //    close(this->adminSocketFD);

    //delete this->batteryListener;
    //if (this->adminListener != -1) {
    //    ::shutdown(this->adminListener, SHUT_RDWR);
    //    close(this->adminListener);
    //}

    //if (this->batteryListener != -1) {
    //    ::shutdown(this->batteryListener, SHUT_RDWR);
    //    close(this->batteryListener);
    //}

    this->directoryManager->destroyDirectory();

    return;
}
