#include <thread>
#include "Admin.hpp"
#include "FifoBattery.hpp"
#include "DynamicBattery.hpp"

std::string path   = "batteries/";
std::string input  = "_fifo_input";
std::string output = "_fifo_output";

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

    const char** initArgs = new const char*[1];
    initArgs[0] = "321";

    if (!admin.createDynamicBattery(initArgs, "DestroyDriverBattery",
                                    "CreateDriverBattery", "DriverRefresh",
                                    "DriverSetCurrent", "dynamic")) {
        ERROR() << "could not create dynamic battery" << std::endl;
    }

    std::this_thread::sleep_for(10s);
    
    admin.shutdown();
    return 0;
}
