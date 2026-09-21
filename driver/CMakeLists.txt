cmake_minimum_required(VERSION 3.15)
project(sts3215_arm CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(servo_tool
    src/main.cpp
    src/serial_port.cpp
    src/sts3215.cpp
)

if(MSVC)
    target_compile_options(servo_tool PRIVATE /W4)
else()
    target_compile_options(servo_tool PRIVATE -Wall -Wextra)
endif()
