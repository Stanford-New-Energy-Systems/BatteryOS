#include <cmath>
#include <chrono>
#include <time.h>
#include <stdint.h>
#include <iostream>
#include <thread>
#include "PhysicalBattery.hpp"

void runTests() {
    using namespace std::chrono_literals;
    PhysicalBattery bat("bat");
    std::cout << bat.getBatteryString() << std::endl;
    std::cout << "Case 1: " << std::endl;
    timepoint_t currentTime = getTimeNow();
    if (!bat.schedule_set_current(200, currentTime+1s, currentTime+3s))
        std::cout << "FALSE" << std::endl;
    bat.schedule_set_current(300, currentTime+2s, currentTime+4s);
    std::this_thread::sleep_for(5s);
    std::cout << "End of case 1" << std::endl; 
    
    std::cout << "Case 2: " << std::endl;
    currentTime = getTimeNow();
    bat.schedule_set_current(200, currentTime+1s, currentTime+4s);
    bat.schedule_set_current(300, currentTime+2s, currentTime+3s);
    std::this_thread::sleep_for(5s);
    std::cout << "End of case 2" << std::endl; 
    
    std::cout << "Case 3: " << std::endl;
    currentTime = getTimeNow();
    bat.schedule_set_current(200, currentTime+2s, currentTime+4s);
    bat.schedule_set_current(300, currentTime+1s, currentTime+3s);
    std::this_thread::sleep_for(5s);
    std::cout << "End of case 3" << std::endl; 

    std::cout << "Case 4: " << std::endl;
    currentTime = getTimeNow();
    bat.schedule_set_current(200, currentTime+1s, currentTime+2s);
    bat.schedule_set_current(300, currentTime+2s, currentTime+3s);
    std::this_thread::sleep_for(5s);
    std::cout << "End of case 4" << std::endl; 

    std::cout << "Case 5: " << std::endl;
    currentTime = getTimeNow();
    bat.schedule_set_current(100, currentTime+100ms, currentTime+5s);
    std::this_thread::sleep_for(2s);
    currentTime = getTimeNow();
    bat.schedule_set_current(200, currentTime+100ms, currentTime+2s);
    std::this_thread::sleep_for(4s);

    std::cout << "done" << std::endl;
    return;
}


int main() {
    runTests();
    return 0;
}   
