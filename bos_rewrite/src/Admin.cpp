#include "Admin.hpp"

Admin::~Admin() {
    if (!this->fifoMode)
        close(this->clientSocket);
};

Admin::Admin(int port, bool TLS) {
    this->fifoMode = false;
    this->TLS      = TLS;

    if (TLS) {
        SSL_library_init();
        this->context = InitCTX();
        if (SSL_CTX_load_verify_locations(this->context, "../certs/ca_cert.pem", NULL) != 1)
            ERR_print_errors_fp(stderr);
        LoadCertificates(this->context, (char*) "../certs/client.pem", (char*) "../certs/client.key");
        SSL_CTX_set_verify(this->context, SSL_VERIFY_PEER, NULL);
    } else {
        this->ssl = nullptr;
        this->context = nullptr;
    }

    this->clientSocket = this->setupClient(port);
    if (!TLS)
        LOG() << "clientSocket = " << this->clientSocket << std::endl;

    if (TLS) {
        this->ssl = SSL_new(this->context);
        SSL_set_fd(this->ssl, this->clientSocket);
        if (SSL_connect(this->ssl) == -1)
            ERR_print_errors_fp(stderr);
    }
}

Admin::Admin(const std::string& inputFilePath, const std::string& outputFilePath) {
    this->fifoMode = true;
    this->inputFilePath  = inputFilePath;
    this->outputFilePath = outputFilePath;
}

/*****************
Private Functions
******************/

SSL_CTX* Admin::InitCTX() {
    OpenSSL_add_all_algorithms();                   // Load crypto
    SSL_load_error_strings();                       // bring in and register error messages
    const SSL_METHOD* method = TLS_client_method(); // create new client-method instance
    SSL_CTX* ctx = SSL_CTX_new(method);             // create new context

    if (ctx == NULL) {
        ERR_print_errors_fp(stderr);
        abort();
    }
    return ctx;
}

void Admin::LoadCertificates(SSL_CTX* ctx, char* certFile, char* keyFile) {
    if (SSL_CTX_use_certificate_file(ctx, certFile, SSL_FILETYPE_PEM) <= 0)
        ERR_print_errors_fp(stderr);
    else if (SSL_CTX_use_PrivateKey_file(ctx, keyFile, SSL_FILETYPE_PEM) <= 0)
        ERR_print_errors_fp(stderr);
    else if (!SSL_CTX_check_private_key(ctx))
        ERR_print_errors_fp(stderr);
    else
        return;
    abort();
}

/****************
Public Functions
*****************/

bool Admin::shutdown() {
    int inputFD;
    int success;
    bosproto::Admin_Command command;
    bosproto::AdminResponse response;
    
    if (this->fifoMode)
        inputFD = open(this->inputFilePath.c_str(), O_WRONLY);
    else
        inputFD = this->clientSocket;

    if (inputFD == -1)
        ERROR() << "could not open input FIFO or output FIFO: check file paths" << std::endl;

    command.set_command_options(bosproto::Command_Options::Shutdown);

    if (this->TLS) {
        int commandSize = command.ByteSizeLong();
        char buffer[commandSize];
        command.SerializeToArray(buffer, commandSize);
        success = SSL_write(this->ssl, buffer, commandSize);
    } else    
        success = command.SerializeToFileDescriptor(inputFD);

    if (!success)
        ERROR() << "error writing message to file descriptor" << std::endl;
    std::cout << " sending shutdown request " << std::endl;

    if (this->fifoMode)
        close(inputFD);
    
    return true;
}

int Admin::setupClient(int port) {
    struct sockaddr_in servAddr;
    int s = socket(AF_INET, SOCK_STREAM, 0);

    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(port);
    servAddr.sin_addr.s_addr = INADDR_ANY; 

    int status = connect(s, (struct sockaddr*)&servAddr, sizeof(servAddr));

    if (status == -1)
        ERROR() << "could not connect to server!" << std::endl;

    return s;
}

bool Admin::createPhysicalBattery(const std::string& name,
                                  const std::chrono::milliseconds& maxStaleness,
                                  const RefreshMode& refreshMode)
{
    LOG() << "entering this function!" << std::endl;
    int bytes;
    int inputFD;
    int success;
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

    command.set_command_options(bosproto::Command_Options::Create_Physical);
    bosproto::Physical_Battery* p = command.mutable_physical_battery(); 
    p->set_batteryname(name);
    p->set_max_staleness(maxStaleness.count());
    
    if (refreshMode == RefreshMode::LAZY)
        p->set_refresh_mode(bosproto::Refresh::LAZY);
    else
        p->set_refresh_mode(bosproto::Refresh::ACTIVE);

    if (this->TLS) {
        LOG() << "about to write command to create physical battery" << std::endl;
        int commandSize = command.ByteSizeLong();
        command.SerializeToArray(buffer, commandSize);
        SSL_write(this->ssl, buffer, commandSize);
    } else {    
        LOG() << "regular socket command begin!" << std::endl; 
        command.SerializeToFileDescriptor(inputFD);
        LOG() << "regular socket command end!" << std::endl; 
    }

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

        if (this->TLS) {
            //while (SSL_peek(this->ssl, buffer, size) == size)
            //    size *= 2;
            bytes = SSL_read(this->ssl, buffer, size);
        } else {
            while (recv(outputFD, buffer, size, MSG_PEEK) == size)
                size *= 2;
            bytes = recv(outputFD, buffer, size, 0);
            LOG() << "in here with the number of bytes = " << bytes << std::endl;
        }
        
        LOG() << "regular response command begin! bytes = " << bytes <<  std::endl;
        success = response.ParseFromArray(buffer, bytes); 
        LOG() << "regular response command end!" << std::endl;
    }

    delete[] fds;

    if (this->fifoMode)
        close(outputFD);

    if (!success) {
        WARNING() << "could not parse admin response" << std::endl;
        return false;
    }

    LOG() << "exiting this function" << std::endl;

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
    int bytes;
    int success;
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

    if (this->TLS) {
        int commandSize = command.ByteSizeLong();
        command.SerializeToArray(buffer, commandSize);
        SSL_write(this->ssl, buffer, commandSize);
    } else
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
        if (this->TLS) {
            //while (SSL_peek(this->ssl, buffer, size) == size)
            //    size *= 2;
            bytes = SSL_read(this->ssl, buffer, size);
        } else {
            while (recv(outputFD, buffer, size, MSG_PEEK) == size)
                size *= 2;
            bytes = recv(outputFD, buffer, size, 0);
        }

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
        return false;
    return true;  

} 
                                    

bool Admin::createAggregateBattery(const std::string& name,
                                   std::vector<std::string> parentNames,
                                   const std::chrono::milliseconds& maxStaleness,
                                   const RefreshMode& refreshMode)
{
    int bytes;
    int success;
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

    if (this->TLS) {
        int commandSize = command.ByteSizeLong();
        command.SerializeToArray(buffer, commandSize);
        SSL_write(this->ssl, buffer, commandSize);
    } else
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
        if (this->TLS) {
            //while (SSL_peek(this->ssl, buffer, size) == size)
            //    size *= 2;
            bytes = SSL_read(this->ssl, buffer, size);
        } else {
            while (recv(outputFD, buffer, size, MSG_PEEK) == size)
                size *= 2;
            bytes = recv(outputFD, buffer, size, 0);
        }

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
        return false;
    return true;  
}

bool Admin::createPartitionBattery(const std::string& sourceName,
                                   const PolicyType &policyType,
                                   std::vector<std::string> names,
                                   std::vector<Scale> child_proportions,
                                   std::vector<std::chrono::milliseconds> maxStalenesses,
                                   std::vector<RefreshMode> refreshModes)
{
    int bytes;
    int success;
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

    if (this->TLS) {
        int commandSize = command.ByteSizeLong();
        command.SerializeToArray(buffer, commandSize);
        SSL_write(this->ssl, buffer, commandSize);
    } else
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
        if (this->TLS) {
            //while (SSL_peek(this->ssl, buffer, size) == size)
            //    size *= 2;
            bytes = SSL_read(this->ssl, buffer, size);
        } else {
            while (recv(outputFD, buffer, size, MSG_PEEK) == size)
                size *= 2;
            bytes = recv(outputFD, buffer, size, 0);
        }

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
        return false;
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
