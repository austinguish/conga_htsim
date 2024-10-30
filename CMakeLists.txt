cmake_minimum_required(VERSION 3.10)
project(htsim)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set compiler flags
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -g -Ofast")
endif()

# Create directories if they don't exist
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/obj)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/data)

# Find all source files
file(GLOB SOURCES "*.cpp")
# add the files in the conga directory
# add the files in the testbed directory
file(GLOB TESTBED_SOURCES "./testbed/*.cpp")

# Create executable
add_executable(${PROJECT_NAME}
        ${SOURCES}
        ${TESTBED_SOURCES}
        testbed/tcp_flow.h
        testbed/switch/ecmp_switch.h
        testbed/switch/constants.h
        testbed/switch/leafswitch.cpp
        testbed/switch/leafswitch.h  # 添加testbed源文件
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/testbed  # 添加testbed目录
)

include_directories(
        ${CMAKE_SOURCE_DIR}        # 添加主目录作为包含路径
)
# Set output directories
set_target_properties(${PROJECT_NAME}
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)