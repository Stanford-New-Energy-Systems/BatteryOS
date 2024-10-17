cmake_minimum_required(VERSION 3.7) 
project (plugins) 
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_library (dynamicnull SHARED dynamicnull.c) 

find_package(CURL REQUIRED)
find_package(RapidJSON REQUIRED)
add_library(sonnen SHARED sonnen.cpp)
target_include_directories(sonnen PRIVATE ${RapidJSON_INCLUDE_DIRS})
target_link_libraries(sonnen PRIVATE ${CURL_LIBRARIES})

add_library(fakedelay SHARED fakedelay.cpp)

