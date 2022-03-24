cmake_minimum_required (VERSION 3.7) 

project (bos) 
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_subdirectory(wiringSerial)
add_subdirectory(protobuf)
set(CXX_SRCS 
    AggregatorBattery.cpp
    BALSplitter.cpp
    BatteryInterface.cpp
    BatteryStatus.cpp 
    BOS.cpp 
    JBDBMS.cpp
    main.cpp 
    ProtobufMsg.cpp 
    RPC.cpp
    util.cpp
)
set(CXX_HEADERS
    AggregatorBattery.hpp 
    BALSplitter.hpp 
    BatteryInterface.hpp 
    BatteryStatus.h 
    BOS.hpp 
    BOSDirectory.hpp 
    BOSNode.hpp 
    Connections.hpp 
    CurrentRegulator.hpp 
    Dynamic.hpp 
    JBDBMS.hpp 
    NetworkBattery.hpp 
    ProtobufMsg.hpp 
    RD6006.hpp 
    RPC.hpp 
    sonnen.hpp 
    SplittedBattery.hpp 
    TestBattery.hpp 
    util.hpp 
)


find_package(CURL REQUIRED)
find_package(Python3 COMPONENTS Development)  # not required 
find_package(RapidJSON REQUIRED)

add_executable(bos ${CXX_SRCS}) 

target_include_directories(bos PUBLIC ${CURL_INCLUDE_DIRS})
if (Python3_FOUND)
    target_include_directories(bos PUBLIC ${Python3_INCLUDE_DIRS})
endif()
target_include_directories(bos PUBLIC ${RapidJSON_INCLUDE_DIRS})
# target_include_directories(bos PUBLIC ${Protobuf_INCLUDE_DIRS}) 
# target_include_directories(bos PUBLIC ${PROTOBUF_MSG_INCLUDE_DIR})
target_link_libraries(bos PUBLIC wiringSerial)
target_link_libraries(bos PUBLIC ${CURL_LIBRARIES})
if (Python3_FOUND)
    target_link_libraries(bos PUBLIC ${Python3_LIBRARIES})
endif()
target_link_libraries(bos PUBLIC protobufmsg)
# target_link_libraries(bos PUBLIC ${Protobuf_LIBRARIES}) 
target_link_libraries(bos ${CMAKE_DL_LIBS})
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.16) 
    target_precompile_headers(bos PUBLIC ${CXX_HEADERS})
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
if (NOT Python3_FOUND)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNO_PYTHON")
endif()

# if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
#     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
# endif()

add_executable(example_client example_client.cpp)
target_link_libraries(example_client PUBLIC protobufmsg)


