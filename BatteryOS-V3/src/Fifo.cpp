
#include "Fifo.hpp"
#include "util.hpp"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

FifoAcceptor::FifoAcceptor(const std::string& path, const std::string& name, std::function<void(FifoPipe*)> connectHandler, bool create)
        : path(path), name(name), connectHandler(connectHandler) {

    if (create) {
        std::string inputPath = path + "_input";
        if (mkfifo(inputPath.c_str(), 0777) == -1) {
            throw std::runtime_error("Unable to create " + inputPath);
        } 

        std::string outputPath = path + "_output";
        if (mkfifo(outputPath.c_str(), 0777) == -1) {
            throw std::runtime_error("Unable to create " + outputPath);
        }
    }

    std::string input_path = path + "_input";
    this->fd = std::optional{ ::open(input_path.c_str(), O_RDONLY | O_NONBLOCK) };
    std::cout << "GOT " << this->fd.value() << std::endl;
    if (this->fd.value() == -1) {
        throw std::runtime_error("Unable to open input fifo " + input_path + " got: " + std::string(std::strerror(errno)));
    }
}

FifoAcceptor::~FifoAcceptor() {
    if (this->fd) {
        std::cout << "CLOSING ACCEPTOR: " << this->fd.value() << std::endl;
        close(*this->fd);
    }
}

FifoAcceptor& FifoAcceptor::operator=(FifoAcceptor&& other) {
    return *this;
}

struct pollfd FifoAcceptor::pollInfo() {
    // Either poll fd or nothing
    return {
        this->fd ? *this->fd : -1,
        POLLIN,
        0
    };
}

void FifoAcceptor::pollHandler() {
    // open the output fd and create new fifo
    std::string output_path = path + "_output";
    int output_fd = ::open(output_path.c_str(), O_WRONLY | O_NONBLOCK);
    if (output_fd == -1) {
        throw std::runtime_error("Unable to open output fifo " + output_path + ": " + std::string(std::strerror(errno)));
    }

    // replace fd with None while the Fifo is alive
    int input_fd = *this->fd;
    this->fd = {};

    // create a new fifo
    FifoPipe* fifo = new FifoPipe(input_fd, output_fd, this, this->name);
    this->connectHandler(fifo);
}

FifoPipe::~FifoPipe() {
    // give input_fd back to acceptor
    if (this->creator) {
        this->creator->fd = std::optional<int>{ this->input_fd };
    }
    if (this->output_fd != -1) {
        std::cout << "CLOSING OUTPUT FD: " << this->output_fd << std::endl;
        close(this->output_fd);
    }
}

FifoPipe& FifoPipe::operator=(FifoPipe&& other) {
    return *this;
}
//
//FifoPipe FifoPipe::open(const std::string& inputPath, const std::string& outputPath) {
//    std::cout << "opening: " << inputPath << " " << outputPath << std::endl;
//    //int input_fd = ::open(inputPath.c_str(), O_RDONLY | O_NONBLOCK);
//    //int output_fd = ::open(outputPath.c_str(), O_WRONLY | O_NONBLOCK);
//    //
//    return FifoPipe(inputPath, outputPath);
//}
FifoPipe FifoPipe::connect(const std::string& directory, const std::string& name) {
    // open the fifos 
    // input/output reversed as we are the connectee
    std::string input_path = directory + "/" + name + "_input";
    std::string output_path = directory + "/" + name + "_output";
    std::cout << "Opening pipe: " << input_path << std::endl;
    int input_fd = ::open(input_path.c_str(), O_WRONLY | O_NONBLOCK);
    int output_fd = ::open(output_path.c_str(), O_RDONLY | O_NONBLOCK);
    std::cout << "OUPUT FD: " << output_fd << std::endl;

    if (input_fd == -1) {
        throw std::runtime_error("Unable to open input fifo " + input_path + ": " + std::string(std::strerror(errno)));
    }

    if (output_fd == -1) {
        throw std::runtime_error("Unable to open output fifo: " + output_path + ": " + std::string(std::strerror(errno)));
    }

    std::cout << "HI THERE" << std::endl;

    return FifoPipe(output_fd, input_fd, nullptr, name);
}

void FifoPipe::setReadHandler(std::function<void()> readHandler) {
    this->readHandler = readHandler;
}

size_t FifoPipe::read(char* buffer, size_t len) {
    return ::read(this->input_fd, buffer, len);
}

size_t FifoPipe::write(char* buffer, size_t len) {
    return ::write(this->output_fd, buffer, len);
}

size_t FifoPipe::read_exact(char* buffer, size_t len) {
    return util::read_exact(this->input_fd, buffer, len);
}

size_t FifoPipe::write_exact(char* buffer, size_t len) {
    return util::write_exact(this->output_fd, buffer, len);
}

struct pollfd FifoPipe::pollInfo() {
    return {
        this->input_fd,
        POLLIN,
        0
    };
}
    
void FifoPipe::pollHandler() {
    this->readHandler();
}
