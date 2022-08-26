#include "BOS.hpp"

/**********************
Constructor/Destructor
***********************/

BOS::~BOS() {
    delete[] this->fds;
    if (!this->hasQuit)
        this->shutdown();
}

BOS::BOS() {
    this->hasQuit          = true;
    this->quitPoll         = false;
    this->adminFifoFD      = -1;
    this->adminSocketFD    = -1;
    this->adminListener    = -1;
    this->batteryListener  = -1;
    this->fds = new pollfd[1028];
    this->directoryManager = std::make_unique<BatteryDirectoryManager>();
} // use only for socket mode

BOS::BOS(const std::string &directoryPath, mode_t permission) : BOS() {
    this->createDirectory(directoryPath, permission);
} // use for both FIFO and socket mode 

/*****************
Private Functions
******************/

void BOS::pollFDs() {
    while(!this->quitPoll) { // think of best way to terminate this loop 
        int index = 4;

        this->fds[0].fd      = this->adminFifoFD;
        this->fds[0].events  = POLLIN;
        this->fds[0].revents = 0;

        this->fds[1].fd      = this->adminSocketFD;
        this->fds[1].events  = POLLIN;
        this->fds[1].revents = 0;

        this->fds[2].fd      = this->adminListener;
        this->fds[2].events  = POLLIN;
        this->fds[2].revents = 0;

        this->fds[3].fd      = this->batteryListener;
        this->fds[3].events  = POLLIN;
        this->fds[3].revents = 0;
        
        for (const auto &p : this->battery_names) {
            this->fds[index].revents = 0;
            this->fds[index].events  = POLLIN;
            this->fds[index++].fd    = p.first;

            if (index == 1024)
                break;
        }
    
        int nelems = poll(this->fds, this->battery_names.size()+4, -1);

        if (nelems == -1)
            ERROR() << "poll failed!" << std::endl;
        else if (nelems != 0) {
            this->checkFileDescriptors();

            if ((this->fds[0].revents & POLLIN) == POLLIN)
                this->handleAdminCommand(this->adminFifoFD, true);
            else if ((this->fds[1].revents & POLLIN) == POLLIN)
                this->handleAdminCommand(this->adminSocketFD, false);
            else if ((this->fds[2].revents & POLLIN) == POLLIN) {
                this->adminSocketFD = accept(this->adminListener, NULL, NULL);
                if (this->adminSocketFD == -1)
                    ERROR() << "error creating client socket from admin socket" << std::endl;
            } else if ((this->fds[3].revents & POLLIN) == POLLIN)
                this->acceptBatteryConnection();
        }
    } 
    return;
}

void BOS::checkFileDescriptors() {
    for (int i = 4; i < this->battery_names.size()+4; i++) {
        if ((this->fds[i].revents & POLLIN) == POLLIN)
            this->handleBatteryCommand(this->fds[i].fd);
    }
    return;
}

void BOS::acceptBatteryConnection() {
    char batteryName[100];
    int clientSocket = accept(this->batteryListener, NULL, NULL); 
    
    int bytes = recv(clientSocket, batteryName, 100, 0);
    batteryName[bytes] = '\0';

    if (bytes == -1)
        WARNING() << "Unable to read batteryName from clientSocket!" << std::endl;
    else if (this->directoryManager->getBattery(batteryName) == nullptr)
        close(clientSocket);
    else
        this->battery_names[clientSocket] = std::make_pair(batteryName, false);

    return;
}

void BOS::handleBatteryCommand(int fd) {
    int success;
    bosproto::BatteryCommand command;
    
    if (this->battery_names[fd].second)
        success = command.ParseFromFileDescriptor(fd);
    else {
        int size = 4096;
        char buffer[size];

        while (recv(fd, buffer, size, MSG_PEEK) == size)
            size *= 2;

        int bytesRead = recv(fd, buffer, size, 0);
        success = command.ParseFromArray(buffer, bytesRead);
    }

    if (!success) {
        WARNING() << "could not parse BatteryCommand" << std::endl;
        return;
    } 

    switch(command.command()) {
        case bosproto::Command::Get_Status:
            this->getStatus(fd);
            break;
        case bosproto::Command::Schedule_Set_Current:
            this->scheduleSetCurrent(command, fd);
            break;
        case bosproto::Command::Remove_Battery:
            this->removeBattery(fd);
            break;
        case bosproto::Command::Set_Status:
            this->setStatus(command, fd);
            break;
        default:
            WARNING() << "need to select battery command option" << std::endl;
            break;
    }

    return;
}

void BOS::getStatus(int fd) {
    int output_fd;
    bosproto::BatteryStatusResponse response;
    bosproto::BatteryStatus* s = response.mutable_status();

    std::string name = this->battery_names[fd].first;
    std::shared_ptr<Battery> bat = this->directoryManager->getBattery(name);
    BatteryStatus status = bat->getStatus();
    
    s->set_voltage_mv(status.voltage_mV);
    s->set_current_ma(status.current_mA);
    s->set_capacity_mah(status.capacity_mAh);
    s->set_max_capacity_mah(status.max_capacity_mAh);
    s->set_max_charging_current_ma(status.max_charging_current_mA);
    s->set_max_discharging_current_ma(status.max_discharging_current_mA);
    s->set_timestamp(status.timestamp.getMilliseconds());

    if (this->battery_names[fd].second) {
        std::string outputFile = this->fileNames[fd].second;
        output_fd = open(outputFile.c_str(), O_WRONLY);
    } else
        output_fd = fd;

    if (output_fd == -1) {
        WARNING() << "Unable to open " << name << "'s output FIFO!" << std::endl;
        return;
    }

    response.set_return_code(0);    
    response.SerializeToFileDescriptor(output_fd);

    if (this->battery_names[fd].second)
        close(output_fd); 

    return;
}

void BOS::setStatus(const bosproto::BatteryCommand& command, int fd) {
    int output_fd;
    BatteryStatus status;
    std::shared_ptr<Battery> battery;
    bosproto::SetStatusResponse response;

    std::string name = this->battery_names[fd].first;
    
    if (this->battery_names[fd].second) {
        std::string outputFile = this->fileNames[fd].second;
        output_fd = open(outputFile.c_str(), O_WRONLY);
    } else
        output_fd = fd;

    if (output_fd == -1) {
        WARNING() << "cannot open " << name << "'s output FIFO" << std::endl; 
        return;
    }
    
    if (!command.has_status()) {
        response.set_return_code(-1);
        response.set_reason("status needs to be set!");

        response.SerializeToFileDescriptor(output_fd);
        
        if (this->battery_names[fd].second)
            close(output_fd);
        return;
    }

    bosproto::BatteryStatus s = command.status();

    status.voltage_mV = s.voltage_mv();
    status.current_mA = s.current_ma();
    status.capacity_mAh = s.capacity_mah();
    status.max_capacity_mAh = s.max_capacity_mah();
    status.max_charging_current_mA = s.max_charging_current_ma();
    status.max_discharging_current_mA = s.max_discharging_current_ma();
    status.timestamp = Timestamp(s.timestamp());

    battery = this->directoryManager->getBattery(name);

    if (battery == nullptr) {
        response.set_return_code(-1);
        response.set_reason("battery does not exist in directory!");
    } else {
        battery->setBatteryStatus(status);
        response.set_return_code(0);
        response.set_reason("successfully set status!");
    }

    response.SerializeToFileDescriptor(output_fd);
    
    if (this->battery_names[fd].second)
        close(output_fd);
    
    return;
}

void BOS::scheduleSetCurrent(const bosproto::BatteryCommand& command, int fd) {
    int output_fd;
    bosproto::ScheduleSetCurrentResponse response;

    std::string name = this->battery_names[fd].first;

    if (this->battery_names[fd].second) {
        std::string outputFile = this->fileNames[fd].second;
        output_fd = open(outputFile.c_str(), O_WRONLY);
    } else 
        output_fd = fd;

    if (output_fd == -1) {
        WARNING() << "cannot open " << name << "'s output FIFO" << std::endl; 
        return;
    }

    if (!command.has_schedule_set_current()) {
        response.set_return_code(-1);
        response.set_failure_message("must set current, start time, and end time");
        
        response.SerializeToFileDescriptor(output_fd);

        if (this->battery_names[fd].second)
            close(output_fd);
        return;
    } 

    bosproto::ScheduleSetCurrent params = command.schedule_set_current();
    double current_mA  = params.current_ma();
    uint64_t startTime = params.starttime();
    uint64_t endTime   = params.endtime(); 

    std::shared_ptr<Battery> bat = this->directoryManager->getBattery(name);
    bool success = bat->schedule_set_current(current_mA, startTime, endTime);

    if (!success) {
        response.set_return_code(-1);
        response.set_failure_message("failure setting current for " + name);
    } else {
        response.set_return_code(0);
        response.set_success_message("successfully set current for " + name);
    }

    if (!response.SerializeToFileDescriptor(output_fd))
        WARNING() << "Unable to serialize response to " << name << std::endl;

    if (this->battery_names[fd].second)
        close(output_fd);

    return;
}

void BOS::removeBattery(int fd) {
    int output_fd;
    bosproto::RemoveBatteryResponse response;

    std::string name = this->battery_names[fd].first;

    if (this->battery_names[fd].second) {
        std::string outputFile = this->fileNames[fd].second;
        output_fd = open(outputFile.c_str(), O_WRONLY);
    } else
        output_fd = fd;

    if (output_fd == -1) {
        WARNING() << "Unable to open " << name << "'s output FIFO" << std::endl;
        return;
    }
    
    bool success = this->directoryManager->removeBattery(name);

    if (!success) {
        response.set_return_code(-1);
        response.set_failure_message("Unable to remove battery");
    } else {
        response.set_return_code(0);
        response.set_success_message("Successfully removed battery");

        close(fd);
        this->battery_names.erase(fd);
        this->fileNames.erase(fd);  
    }

    if (!response.SerializeToFileDescriptor(output_fd))
        WARNING() << "unable to write remove battery response to output FIFO" << std::endl;

    if (this->battery_names[fd].second)
        close(output_fd);

    return;
}

void BOS::handleAdminCommand(int fd, bool FIFO) {
    int success;
    bosproto::Admin_Command command;
    
    if (FIFO)
        success = command.ParseFromFileDescriptor(fd);
    else {
        int size = 4096;
        char buffer[size];

        while (recv(fd, buffer, size, MSG_PEEK) == size)
            size *= 2;

        int bytesRead = recv(fd, buffer, size, 0);
        
        success = command.ParseFromArray(buffer, bytesRead);
    }

    if (!success) {
        WARNING() << "could not parse Admin_Command" << std::endl;
        return;
    }

    switch(command.command_options()) {
        case bosproto::Command_Options::Create_Physical:
            this->createPhysicalBattery(command, fd, FIFO); 
            break;
        case bosproto::Command_Options::Create_Aggregate:
            this->createAggregateBattery(command, fd, FIFO);
            break;
        case bosproto::Command_Options::Create_Partition:
            this->createPartitionBattery(command, fd, FIFO);
            break;
        case bosproto::Command_Options::Shutdown:
            this->shutdown();
            break;
        default:
            WARNING() << "invalid Command_Options" << std::endl;
            break;
    }
    return;
}

void BOS::createPhysicalBattery(const bosproto::Admin_Command& command, int fd, bool FIFO) {
    int output_fd;
    bosproto::AdminResponse response;

    if (FIFO) {
        std::string adminOutputFile = this->fileNames[this->adminFifoFD].second;
        output_fd = open(adminOutputFile.c_str(), O_WRONLY);
    } else
        output_fd = fd;

    if (!command.has_physical_battery()) {
        response.set_return_code(-1);
        response.set_failure_message("Physical_Battery parameters not set!");
        
        if (response.SerializeToFileDescriptor(output_fd))
            WARNING() << "unable to write failure message to file descriptor" << std::endl;

        if (FIFO)
            close(output_fd);

        return;
    }

    paramsPhysical b = parsePhysicalBattery(command.physical_battery());
    std::shared_ptr<Battery> bat = this->directoryManager->createPhysicalBattery(b.name, b.staleness, b.refresh);

    if (bat != nullptr) {
        response.set_return_code(0);
        response.set_success_message("successfully created battery: " + b.name);

        if (FIFO) {
            std::string inputFile  = this->directoryPath + b.name + "_fifo_input";
            std::string outputFile = this->directoryPath + b.name + "_fifo_output";

            int input_fd = this->createFifos(inputFile, outputFile, 0777);
            this->battery_names[input_fd] = std::make_pair(b.name, true);
        }
    } else {
        response.set_return_code(-1);
        response.set_failure_message("battery name: " +  b.name + " already exists");
    }

    if (!response.SerializeToFileDescriptor(output_fd))
        WARNING() << "unable to write message to file descriptor" << std::endl;

    if (FIFO)
        close(output_fd);

    return;
}

void BOS::createAggregateBattery(const bosproto::Admin_Command& command, int fd, bool FIFO) {
    int output_fd;
    bosproto::AdminResponse response;

    if (FIFO) {
        std::string adminOutputFile = this->fileNames[this->adminFifoFD].second;
        output_fd = open(adminOutputFile.c_str(), O_WRONLY);
    } else
        output_fd = fd;

    if (!command.has_aggregate_battery()) {
        response.set_return_code(-1);
        response.set_failure_message("Aggregate_Battery parameters not set!");
        
        if (response.SerializeToFileDescriptor(output_fd))
            WARNING() << "unable to write failure message to file descriptor" << std::endl;

        if (FIFO)
            close(output_fd);

        return;
    }

    paramsAggregate b = parseAggregateBattery(command.aggregate_battery());
    std::shared_ptr<Battery> bat = this->directoryManager->createAggregateBattery(b.name, b.parents, b.staleness, b.refresh);     

    if (bat != nullptr) {
        response.set_return_code(0);
        response.set_success_message("successfully created battery: " + b.name);

        if (FIFO) {
            std::string inputFile  = this->directoryPath + b.name + "_fifo_input";
            std::string outputFile = this->directoryPath + b.name + "_fifo_output";

            int input_fd = this->createFifos(inputFile, outputFile, 0777);
            this->battery_names[input_fd] = std::make_pair(b.name, true);
        }
    } else {
        response.set_return_code(-1);
        response.set_failure_message("error creating aggregate battery!");
    }

    if (!response.SerializeToFileDescriptor(output_fd))
        WARNING() << "unable to write message to file descriptor" << std::endl;

    if (FIFO)
        close(output_fd);

    return;
}

void BOS::createPartitionBattery(const bosproto::Admin_Command& command, int fd, bool FIFO) {
    int output_fd;
    bosproto::AdminResponse response;
    std::vector<std::shared_ptr<Battery>> batteries;

    if (FIFO) {
        std::string adminOutputFile = this->fileNames[this->adminFifoFD].second;
        output_fd = open(adminOutputFile.c_str(), O_WRONLY);
    } else
        output_fd = fd;

    if (!command.has_partition_battery()) {
        response.set_return_code(-1);
        response.set_failure_message("Partition_Battery parameters not set!");
        
        if (response.SerializeToFileDescriptor(output_fd))
            WARNING() << "unable to write failure message to file descriptor" << std::endl;

        if (FIFO)
            close(output_fd);

        return;
    }

    paramsPartition b = parsePartitionBattery(command.partition_battery());

    if (b.refreshModes.size() == 0 && b.stalenesses.size() == 0)
        batteries = this->directoryManager->createPartitionBattery(b.source, b.policy, b.child_names, b.proportions); 
    else if (b.refreshModes.size() == 0 || b.stalenesses.size() == 0) {
        response.set_return_code(-1);
        response.set_failure_message("refresh modes and stalenesses both need to be included or excluded: cannot choose one");

        if (response.SerializeToFileDescriptor(output_fd))
            WARNING() << "unable to write failure message to file descriptor" << std::endl;

        if (FIFO)
            close(output_fd);

        return;
    } else
        batteries = this->directoryManager->createPartitionBattery(b.source, b.policy, b.child_names, b.proportions, b.stalenesses, b.refreshModes); 

    if (batteries.size() != 0) {
        response.set_return_code(0);
        response.set_success_message("successfully created batteries!");

        if (FIFO) {
            for (int i = 0; i < b.child_names.size(); i++) {
                std::string inputFile  = this->directoryPath + b.child_names[i] + "_fifo_input";
                std::string outputFile = this->directoryPath + b.child_names[i] + "_fifo_output";

                int input_fd = this->createFifos(inputFile, outputFile, 0777);
                this->battery_names[input_fd] = std::make_pair(b.child_names[i], true);
            }
        }
    } else {
        response.set_return_code(-1);
        response.set_failure_message("error creating partition batteries!");
    }

    if (!response.SerializeToFileDescriptor(output_fd))
        WARNING() << "unable to write message to file descriptor" << std::endl;

    if (FIFO)
        close(output_fd);

    return;
    
}

void BOS::createDirectory(const std::string &directoryPath, mode_t permission) {
    if (mkdir(directoryPath.c_str(), permission) == -1)
        WARNING() << directoryPath << " already exists" << std::endl;
    this->directoryPath = directoryPath + "/";
    return;
}

int BOS::createFifos(const std::string& inputFile, const std::string& outputFile, mode_t permission) {
    if (mkfifo(inputFile.c_str(), permission) == -1)
        ERROR() << "Unable to create " << inputFile << std::endl;
    else if (mkfifo(outputFile.c_str(), permission) == -1)
        ERROR() << "Unable to create " << outputFile << std::endl;

    int fd = open(inputFile.c_str(), O_RDONLY | O_NONBLOCK);

    if (fd == -1)
        ERROR() << "Unable to open " << inputFile << std::endl;
    this->fileNames[fd] = std::make_pair(inputFile, outputFile);

    return fd;
}

/****************
Public Functions
*****************/

void BOS::startFifos(mode_t adminPermission) {
    this->hasQuit = false;

    std::string inputFile  = this->directoryPath + "admin_fifo_input";
    std::string outputFile = this->directoryPath + "admin_fifo_output";
    
    this->adminFifoFD = this->createFifos(inputFile, outputFile, adminPermission);

    this->pollFDs();

    return;
}

void BOS::startSockets(int adminPort, int batteryPort) {
    this->adminListener   = socket(AF_INET, SOCK_STREAM, 0);
    this->batteryListener = socket(AF_INET, SOCK_STREAM, 0);

    if (this->adminListener < 0 || this->batteryListener < 0)
        ERROR() << "failed to create admin/battery socket" << std::endl;   

    struct sockaddr_in adminAddr;
    struct sockaddr_in batteryAddr;

    adminAddr.sin_port          = htons(adminPort);
    adminAddr.sin_family        = AF_INET;
    adminAddr.sin_addr.s_addr   = INADDR_ANY;

    batteryAddr.sin_port        = htons(batteryPort);
    batteryAddr.sin_family      = AF_INET;
    batteryAddr.sin_addr.s_addr = INADDR_ANY;    

    if (bind(this->adminListener, (struct sockaddr*)&adminAddr, sizeof(adminAddr)) < 0)
        ERROR() << "failed to bind to adminListener" << std::endl;
    else if (bind(this->batteryListener, (struct sockaddr*)&batteryAddr, sizeof(batteryAddr)) < 0)
        ERROR() << "failed to bind on batteryListener" << std::endl;
    else if (listen(this->adminListener, 1) < 0)
        ERROR() << "failed to listen on adminSocket: " << this->adminListener << std::endl;
    else if (listen(this->batteryListener, 1024) < 0)
        ERROR() << "failed to listen on batterySocket: " << this->batteryListener << std::endl;

    if (this->hasQuit) {
        this->hasQuit = false;
        this->pollFDs();
    }

    return;
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

    for (const auto &f: battery_names) {
        if (f.second.second)
            close(f.first);
    }

    if (this->adminSocketFD != -1)
        close(this->adminSocketFD);

    if (this->adminListener != -1) {
        ::shutdown(this->adminListener, SHUT_RDWR);
        close(this->adminListener);
    }

    if (this->batteryListener != -1) {
        ::shutdown(this->batteryListener, SHUT_RDWR);
        close(this->batteryListener);
    }

    return;
}
