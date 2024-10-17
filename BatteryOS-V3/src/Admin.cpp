#include "Admin.hpp"

#include "Fifo.hpp"
#include "TLSSocket.hpp"

Admin::~Admin() {
    //if (!this->fifoMode)
        //close(this->clientSocket);
};

Admin::Admin(int port) {
    this->fifoMode = false;
    this->clientSocket = this->setupClient(port);
}

Admin::Admin(const std::string& path) : path(path), fifoMode(true) {
    FifoPipe fifo = FifoPipe::connect("batteries", path);
    this->clientSocket = new BatteryConnection(std::make_unique<FifoPipe>(std::move(fifo)));
}

/****************
Public Functions
*****************/

bool Admin::shutdown() {
    //int inputFD;
    bosproto::Admin_Command command;
    command.set_command_options(bosproto::Command_Options::Shutdown);
    int success = this->clientSocket->write(command);
    if (!success)
        ERROR() << "error writing message to file descriptor" << std::endl;
    std::cout << " sending shutdown request " << std::endl;
    //
    //if (this->fifoMode)
    //    inputFD  = open(this->inputFilePath.c_str(), O_WRONLY);
    //else
    //    inputFD = this->clientSocket;

    //if (inputFD == -1)
    //    ERROR() << "could not open input FIFO or output FIFO: check file paths" << std::endl;


    //if (this->fifoMode)
    //    close(inputFD);
    //
    return true;
}

BatteryConnection* Admin::setupClient(int port) {
    TLSSocket::InitializeClient("../certs/ca_cert.pem", "../certs/client.pem", "../certs/client.key");
    struct sockaddr_in servAddr;
    int s = socket(AF_INET, SOCK_STREAM, 0);

    std::unique_ptr<TLSSocket> socket = TLSSocket::connect(s, INADDR_ANY, port);
    BatteryConnection* b = new BatteryConnection(std::move(socket));

    return b;
}

bool Admin::createPhysicalBattery(const std::string& name,
                                  const std::chrono::milliseconds& maxStaleness,
                                  const RefreshMode& refreshMode)
{
    bosproto::Admin_Command command;
    bosproto::AdminResponse response;

    command.set_command_options(bosproto::Command_Options::Create_Physical);
    bosproto::Physical_Battery* p = command.mutable_physical_battery(); 
    p->set_batteryname(name);
    p->set_max_staleness(maxStaleness.count());
    
    if (refreshMode == RefreshMode::LAZY)
        p->set_refresh_mode(bosproto::Refresh::LAZY);
    else
        p->set_refresh_mode(bosproto::Refresh::ACTIVE);
   
    this->clientSocket->write(command);

    int success = this->clientSocket->read(response); 

    if (!success) {
        WARNING() << "could not parse admin response" << std::endl;
        return false;
    }

    if (response.return_code() == -1)
        return false;
    return true;  
} 

bool Admin::createDynamicBattery(const char** initArgs,
                                 const std::string& destructor,
                                 const std::string& constructor,
                                 const std::string& refreshFunc,
                                 const std::string& setCurrentFunc,
                                 const std::string& name,
                                 const unsigned int argsLen,
                                 const std::chrono::milliseconds& maxStaleness, 
                                 const RefreshMode& refreshMode)
{
/*    int success;
    int inputFD;
    int outputFD;
    int size = 4096;
    char buffer[size];
    pollfd* fds = new pollfd[1];
    bosproto::Admin_Command command;
    bosproto::AdminResponse response;

    if (this->fifoMode) {
        inputFD  = open(this->inputFilePath.c_str(), O_WRONLY);
        outputFD = open(this->outputFilePath.c_str(), O_RDONLY | O_NONBLOCK);
    } else {
        inputFD  = this->clientSocket;
        outputFD = this->clientSocket;
    }

    if (inputFD == -1 || outputFD == -1)
        ERROR() << "could not open input FIFO or output FIFO: check file paths" << std::endl;

    fds[0].fd      = outputFD;
    fds[0].events  = POLLIN;
    fds[0].revents = 0;

    command.set_command_options(bosproto::Command_Options::Create_Dynamic);
    bosproto::Dynamic_Battery* d = command.mutable_dynamic_battery(); 
    d->set_batteryname(name);
    d->set_max_staleness(maxStaleness.count());
    
    if (refreshMode == RefreshMode::LAZY)
        d->set_refresh_mode(bosproto::Refresh::LAZY);
    else
        d->set_refresh_mode(bosproto::Refresh::ACTIVE);

    for (unsigned int i = 0; i < argsLen; i++) {
        LOG() << initArgs[i] << std::endl;
        d->add_arguments(initArgs[i]);
    }

    d->set_refresh_func(refreshFunc);
    d->set_destructor_func(destructor);
    d->set_constructor_func(constructor);
    d->set_set_current_func(setCurrentFunc); 
    
    LOG() << "about to serialize" << std::endl;

    command.SerializeToFileDescriptor(inputFD);

    LOG() << "after serialization" << std::endl;

    if (this->fifoMode)
        close(inputFD);

    while ((fds[0].revents & POLLIN) != POLLIN) {
        success = poll(fds, 1, -1);
        if (success == -1)
            ERROR() << "poll failed!" << std::endl;
    } 

    if (this->fifoMode)
        success = response.ParseFromFileDescriptor(outputFD);
    else {

        while (recv(outputFD, buffer, size, MSG_PEEK) == size)
            size *= 2;

        int bytes = recv(outputFD, buffer, size, 0);
        success = response.ParseFromArray(buffer, bytes); 
    }

    delete[] fds;

    if (this->fifoMode)
        close(outputFD);

    if (!success) {
        WARNING() << "could not parse admin response" << std::endl;
        return false;
    }

    if (response.return_code() == -1)
        return false;*/
    return true;  

} 
                                    

bool Admin::createAggregateBattery(const std::string& name,
                                   std::vector<std::string> parentNames,
                                   const std::chrono::milliseconds& maxStaleness,
                                   const RefreshMode& refreshMode)
{
    bosproto::Admin_Command command;
    command.set_command_options(bosproto::Command_Options::Create_Aggregate);
    bosproto::Aggregate_Battery* a = command.mutable_aggregate_battery(); 

    a->set_batteryname(name);
    a->set_max_staleness(maxStaleness.count());

    for (unsigned int i = 0; i < parentNames.size(); i++)
        a->add_parentnames(parentNames[i]);
    
    if (refreshMode == RefreshMode::LAZY)
        a->set_refresh_mode(bosproto::Refresh::LAZY);
    else
        a->set_refresh_mode(bosproto::Refresh::ACTIVE);

    this->clientSocket->write(command);

    bosproto::AdminResponse response;
    int success = this->clientSocket->read(response);
    if (!success) {
        WARNING() << "could not parse admin response" << std::endl;
        return false;
    }

    if (response.return_code() == -1)
        return false;

    return true;


    /*int success;
    int inputFD;
    int outputFD;
    int size = 4096;
    char buffer[size];
    pollfd* fds = new pollfd[1];
    bosproto::Admin_Command command;
    bosproto::AdminResponse response;

    if (this->fifoMode) {
        inputFD  = open(this->inputFilePath.c_str(), O_WRONLY);
        outputFD = open(this->outputFilePath.c_str(), O_RDONLY | O_NONBLOCK);
    } else {
        inputFD  = this->clientSocket;
        outputFD = this->clientSocket;
    }
    
    if (inputFD == -1 || outputFD == -1)
        ERROR() << "could not open input FIFO or output FIFO: check file paths" << std::endl;

    fds[0].fd      = outputFD;
    fds[0].events  = POLLIN;
    fds[0].revents = 0;

    command.set_command_options(bosproto::Command_Options::Create_Aggregate);
    bosproto::Aggregate_Battery* a = command.mutable_aggregate_battery(); 

    a->set_batteryname(name);
    a->set_max_staleness(maxStaleness.count());

    for (unsigned int i = 0; i < parentNames.size(); i++)
        a->add_parentnames(parentNames[i]);
    
    if (refreshMode == RefreshMode::LAZY)
        a->set_refresh_mode(bosproto::Refresh::LAZY);
    else
        a->set_refresh_mode(bosproto::Refresh::ACTIVE);

    command.SerializeToFileDescriptor(inputFD);

    if (this->fifoMode)
        close(inputFD);

    while ((fds[0].revents & POLLIN) != POLLIN) {
        success = poll(fds, 1, -1);
        if (success == -1)
            ERROR() << "poll failed!" << std::endl;
    } 

    if (this->fifoMode)
        success = response.ParseFromFileDescriptor(outputFD);
    else {

        while (recv(outputFD, buffer, size, MSG_PEEK) == size)
            size *= 2;

        int bytes = recv(outputFD, buffer, size, 0);
        success = response.ParseFromArray(buffer, bytes); 
    }

    delete[] fds;

    if (this->fifoMode)
        close(outputFD);

    if (!success) {
        WARNING() << "could not parse admin response" << std::endl;
        return false;
    }

    if (response.return_code() == -1)
        return false;*/
    return true;  
}

bool Admin::createPartitionBattery(const std::string& sourceName,
                                   const PolicyType &policyType,
                                   std::vector<std::string> names,
                                   std::vector<Scale> child_proportions,
                                   std::vector<std::chrono::milliseconds> maxStalenesses,
                                   std::vector<RefreshMode> refreshModes)
{
    bosproto::Admin_Command command;
    command.set_command_options(bosproto::Command_Options::Create_Partition);
    bosproto::Partition_Battery* p = command.mutable_partition_battery(); 

    p->set_sourcename(sourceName);
    
    if (policyType == PolicyType::PROPORTIONAL)
        p->set_policy(bosproto::Policy::PROPORTIONAL);
    else if (policyType == PolicyType::TRANCHED)
        p->set_policy(bosproto::Policy::TRANCHED);
    else
        p->set_policy(bosproto::Policy::RESERVED);

    for (unsigned int i = 0; i < names.size(); i++)
        p->add_names(names[i]);

    for (unsigned int i = 0; i < child_proportions.size(); i++) {
        bosproto::Scale* s = p->add_scales();
        double charge   = child_proportions[i].charge_proportion; 
        double capacity = child_proportions[i].capacity_proportion;
        s->set_charge_proportion(charge);
        s->set_capacity_proportion(capacity);
    }
    
    for (unsigned int i = 0; i < maxStalenesses.size(); i++)
        p->add_max_stalenesses(maxStalenesses[i].count());

    for (unsigned int i = 0; i < refreshModes.size(); i++) {
        bosproto::Refresh r;

        if (refreshModes[i] == RefreshMode::LAZY)
            r = bosproto::Refresh::LAZY;
        else
            r = bosproto::Refresh::ACTIVE; 

        p->add_refresh_modes(r); 
    }

    this->clientSocket->write(command);

    bosproto::AdminResponse response;
    int success = this->clientSocket->read(response); 

    if (!success) {
        WARNING() << "could not parse admin response" << std::endl;
        return false;
    }

    if (response.return_code() == -1)
        return false;

    return true;
    /*int success;
    int inputFD;
    int outputFD;
    int size = 4096;
    char buffer[size];
    pollfd* fds = new pollfd[1];
    bosproto::Admin_Command command;
    bosproto::AdminResponse response;

    if (this->fifoMode) {
        inputFD  = open(this->inputFilePath.c_str(), O_WRONLY);
        outputFD = open(this->outputFilePath.c_str(), O_RDONLY | O_NONBLOCK);
    } else {
        inputFD  = this->clientSocket;
        outputFD = this->clientSocket;
    }

    if (inputFD == -1 || outputFD == -1)
        ERROR() << "could not open input FIFO or output FIFO: check file paths" << std::endl;

    fds[0].fd      = outputFD;
    fds[0].events  = POLLIN;
    fds[0].revents = 0;

    command.set_command_options(bosproto::Command_Options::Create_Partition);
    bosproto::Partition_Battery* p = command.mutable_partition_battery(); 

    p->set_sourcename(sourceName);
    
    if (policyType == PolicyType::PROPORTIONAL)
        p->set_policy(bosproto::Policy::PROPORTIONAL);
    else if (policyType == PolicyType::TRANCHED)
        p->set_policy(bosproto::Policy::TRANCHED);
    else
        p->set_policy(bosproto::Policy::RESERVED);

    for (unsigned int i = 0; i < names.size(); i++)
        p->add_names(names[i]);

    for (unsigned int i = 0; i < child_proportions.size(); i++) {
        bosproto::Scale* s = p->add_scales();
        double charge   = child_proportions[i].charge_proportion; 
        double capacity = child_proportions[i].capacity_proportion;
        s->set_charge_proportion(charge);
        s->set_capacity_proportion(capacity);
    }
    
    for (unsigned int i = 0; i < maxStalenesses.size(); i++)
        p->add_max_stalenesses(maxStalenesses[i].count());

    for (unsigned int i = 0; i < refreshModes.size(); i++) {
        bosproto::Refresh r;

        if (refreshModes[i] == RefreshMode::LAZY)
            r = bosproto::Refresh::LAZY;
        else
            r = bosproto::Refresh::ACTIVE; 

        p->add_refresh_modes(r); 
    }

    command.SerializeToFileDescriptor(inputFD);

    if (this->fifoMode)
        close(inputFD);

    while ((fds[0].revents & POLLIN) != POLLIN) {
        success = poll(fds, 1, -1);
        if (success == -1)
            ERROR() << "poll failed!" << std::endl;
    } 

    if (this->fifoMode)
        success = response.ParseFromFileDescriptor(outputFD);
    else {

        while (recv(outputFD, buffer, size, MSG_PEEK) == size)
            size *= 2;

        int bytes = recv(outputFD, buffer, size, 0);
        success = response.ParseFromArray(buffer, bytes); 
    }

    delete[] fds;

    if (this->fifoMode)
        close(outputFD);

    if (!success) {
        WARNING() << "could not parse admin response" << std::endl;
        return false;
    }

    if (response.return_code() == -1)
        return false;*/
    return true;  
}


bool Admin::createPartitionBattery(const std::string& sourceName,
                                   const PolicyType &policyType,
                                   std::vector<std::string> names,
                                   std::vector<Scale> child_proportions)
{
    std::vector<RefreshMode> r;
    std::vector<std::chrono::milliseconds> m;

    return this->createPartitionBattery(sourceName, policyType, names, child_proportions, m, r);
}

bool Admin::createSecureBattery(const std::string& name, uint64_t num_clients)
{
    bosproto::Admin_Command command;
    bosproto::AdminResponse response;

    command.set_command_options(bosproto::Command_Options::Create_Secure);
    bosproto::Secure_Battery* p = command.mutable_secure_battery(); 
    p->set_batteryname(name);
    p->set_numclients(num_clients);
    
    this->clientSocket->write(command);

    int success = this->clientSocket->read(response); 

    if (!success) {
        WARNING() << "could not parse admin response" << std::endl;
        return false;
    }

    if (response.return_code() == -1)
        return false;
    return true;  
}
