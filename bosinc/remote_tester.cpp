#include <unistd.h> 
#include <fcntl.h>
#include <iostream>
#include <string> 
#include "Remote.hpp"
int main() {
    RemoteBattery remote("local", "remote", "127.0.0.1", 1234, std::chrono::milliseconds(1000));
    std::cout << remote.manual_refresh() << std::endl;
    return 0;  
}
