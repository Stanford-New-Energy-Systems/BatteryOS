#include <thread>
#include "Admin.hpp"
#include "FifoBattery.hpp"
#include "DynamicBattery.hpp"

std::string path   = "batteries/";
std::string input  = "_fifo_input";
std::string output = "_fifo_output";

#define ARGS_SIZE 4

using timepoint_t = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>;

timepoint_t getTime() {
    return std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()); 
}

std::string makeInputFileName(const std::string& name) {
    return path + name + input;
}

std::string makeOutputFileName(const std::string& name) {
    return path + name + output;
}

int main(void) {
    using namespace std::chrono_literals;

    Admin admin(makeInputFileName("admin"), makeOutputFileName("admin"));

    const char** initArgs = new const char*[ARGS_SIZE];
    initArgs[0] = "/dev/tty.usbserial-0001";
    initArgs[1] = "9600";
    initArgs[2] = "200000";
    initArgs[3] = "200000";

    if (!admin.createDynamicBattery(initArgs, "DestroyJBDBMS",
                                    "CreateJBDBMS", "JBDBMSRefresh",
                                    "JBDBMSSetCurrent", "dynamic", ARGS_SIZE)) {
        ERROR() << "could not create dynamic battery" << std::endl;
    }


    std::this_thread::sleep_for(15s);
    
    admin.shutdown();
    return 0;
}
