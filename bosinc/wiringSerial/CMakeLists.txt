cmake_minimum_required (VERSION 3.7) 
project (wiringSerial) 
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_library (wiringSerial wiringSerial.c) 
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.16) 
    target_precompile_headers(wiringSerial PUBLIC wiringSerial.h)
endif()





