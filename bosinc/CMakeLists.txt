cmake_minimum_required (VERSION 3.0) 

project (bos) 
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_subdirectory(wiringSerial)
set(CXX_SRCS 
    AggregatorBattery.cpp
    BALSplitter.cpp
    BatteryInterface.cpp
    BatteryStatus.cpp 
    BOS.cpp 
    JBDBMS.cpp
    main.cpp 
    RPC.cpp
    util.cpp
)
find_package(CURL REQUIRED)
find_package(Python3 COMPONENTS Development REQUIRED)
find_package(RapidJSON REQUIRED)

add_executable(bos ${CXX_SRCS}) 


target_include_directories(bos PUBLIC ${CURL_INCLUDE_DIRS})
target_include_directories(bos PUBLIC ${Python3_INCLUDE_DIRS})
target_include_directories(bos PUBLIC ${RapidJSON_INCLUDE_DIRS})
target_link_libraries(bos PUBLIC wiringSerial)
target_link_libraries(bos PUBLIC ${CURL_LIBRARIES})
target_link_libraries(bos PUBLIC ${Python3_LIBRARIES})

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()



