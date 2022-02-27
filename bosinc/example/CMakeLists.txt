cmake_minimum_required(VERSION 3.9) 
project (dynamicnull) 
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_library (dynamicnull SHARED dynamicnull.c) 


