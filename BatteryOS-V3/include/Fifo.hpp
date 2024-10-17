#ifndef BFIFO_HPP
#define BFIFO_HPP

#include "Stream.hpp"
#include <functional>
#include <memory>
#include <utility>
#include <optional>
#include <string>
#include <iostream>

class FifoPipe;

class FifoAcceptor : public Pollable {
    private:
        std::string path;
        std::string name;
        std::function<void(FifoPipe*)> connectHandler;

    public:
        // TODO: make private
        // Some(fd) if we are waiting for a connection
        // None if the connection is being used
        std::optional<int> fd;

        FifoAcceptor(const std::string& path, const std::string& name, std::function<void(FifoPipe*)> connectHandler, bool create);
        virtual ~FifoAcceptor();
        // TODO: write these out fully
        FifoAcceptor(const FifoAcceptor& other) = delete;
        FifoAcceptor(FifoAcceptor&& other) noexcept 
            : path(std::exchange(other.path, "")), 
              name(std::exchange(other.name, "")), 
              fd(std::exchange(other.fd, std::nullopt)),
              connectHandler(std::exchange(other.connectHandler, std::function<void(FifoPipe*)>())) {}
        FifoAcceptor& operator=(const FifoAcceptor& other) = delete;
        // TODO: fix definition
        FifoAcceptor& operator=(FifoAcceptor&& other);

        // Pollable
        struct pollfd pollInfo();
        void pollHandler();

        static FifoAcceptor create(const std::string& path, std::function<void(FifoPipe*)> connectHandler);
        static FifoAcceptor open(const std::string& path);
};

class FifoPipe : public Stream {
    private:
        int input_fd, output_fd;
        std::function<void()> readHandler;

        // null if we have no creator (e.g. created using connect)
        // TODO: pointer could become dangling if we aren't
        //       careful about lifetimes here...
        FifoAcceptor* creator;

    public:
        std::string name;

        FifoPipe(int input_fd, int output_fd, FifoAcceptor* creator, const std::string& name) : input_fd(input_fd), output_fd(output_fd), creator(creator), name(name) {
            std::cout << "CTOR" << std::endl;
        }
        ~FifoPipe();
        FifoPipe(const FifoPipe& other) = delete;
        FifoPipe(FifoPipe&& other) noexcept 
            : input_fd(std::exchange(other.input_fd, -1)), 
              output_fd(std::exchange(other.output_fd, -1)),
              creator(std::exchange(other.creator, nullptr)),
              readHandler(std::exchange(other.readHandler, std::function<void()>())) {}
        FifoPipe& operator=(const FifoPipe& other) = delete;
        // TODO: fix definition
        FifoPipe& operator=(FifoPipe&& other);

        static FifoPipe connect(const std::string& directory, const std::string& name);

        void setReadHandler(std::function<void()> readHandler);

        // Stream
        size_t read(char* buffer, size_t len);
        size_t write(char* buffer, size_t len);
        size_t read_exact(char* buffer, size_t len);
        size_t write_exact(char* buffer, size_t len);


        // Pollable
        struct pollfd pollInfo();
        void pollHandler();
};

#endif
