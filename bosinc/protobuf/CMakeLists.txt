cmake_minimum_required (VERSION 3.7) 

project (protobufmsg) 
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Protobuf REQUIRED)
set(PROTOBUF_FILE_DIR "${PROJECT_SOURCE_DIR}/protobuf")
set(PROTOBUF_MSG_NAMES
    battery_status
    admin_msg
    battery_msg
)
set(PROTO_FILES)
# set(PROTO_CXX_FILES)
# set(PROTO_H_FILES)
# set(PROTOBUF_CXX_OUT_DIR "${PROTOBUF_FILE_DIR}/cppout")

set(PRINT_DEBUG_INFORMATION OFF)

foreach(MSG_NAME ${PROTOBUF_MSG_NAMES})
    # set(PROTO_CXX_FILES ${PROTO_CXX_FILES} "${PROTOBUF_CXX_OUT_DIR}/${MSG_NAME}.pb.cc")
    # set(PROTO_H_FILES ${PROTO_H_FILES} "${PROTOBUF_CXX_OUT_DIR}/${MSG_NAME}.pb.h")
    if (PRINT_DEBUG_INFORMATION)
        message(STATUS "msg name.proto : ${MSG_NAME}.proto")
    endif()
    set(PROTO_FILES ${PROTO_FILES} "${MSG_NAME}.proto") 
endforeach()
if (PRINT_DEBUG_INFORMATION)
    message(STATUS "protofiles : ${PROTO_FILES}")
endif()
protobuf_generate_cpp(PROTO_CXX_FILES PROTO_H_FILES ${PROTO_FILES})
protobuf_generate_python(PROTO_PYTHON_FILES ${PROTO_FILES}) 
if (PRINT_DEBUG_INFORMATION)
    message(STATUS "current bin dir : ${CMAKE_CURRENT_BINARY_DIR}")
    message(STATUS "PROTO_CXX_FILES : ${PROTO_CXX_FILES}") 
    message(STATUS "PROTO_H_FILES : ${PROTO_H_FILES}")
    # message(STATUS "PROTO_PYTHON_FILES : ${PROTO_PYTHON_FILES}")
endif()
add_library (protobufmsg ${PROTO_CXX_FILES}) 
target_include_directories(protobufmsg PUBLIC ${Protobuf_INCLUDE_DIRS}) 
target_include_directories(protobufmsg PUBLIC ${CMAKE_CURRENT_BINARY_DIR}) 
target_link_libraries(protobufmsg PUBLIC ${Protobuf_LIBRARIES}) 
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.16) 
    target_precompile_headers(protobufmsg PUBLIC ${PROTO_H_FILES}) 
endif() 
set(PROTOBUF_MSG_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR})
if (PRINT_DEBUG_INFORMATION)
    message(STATUS "PROTOBUF_MSG_INCLUDE_DIR : ${PROTOBUF_MSG_INCLUDE_DIR}")
endif()



