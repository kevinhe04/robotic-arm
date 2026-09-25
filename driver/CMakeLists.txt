cmake_minimum_required(VERSION 3.15)
project(sts3215_arm CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    set(SERIAL_PORT_SOURCE src/serial_port_win32.cpp)
else()
    set(SERIAL_PORT_SOURCE src/serial_port_posix.cpp)
endif()

add_executable(servo_tool
    src/main.cpp
    src/sts3215.cpp
    ${SERIAL_PORT_SOURCE}
)

if(MSVC)
    target_compile_options(servo_tool PRIVATE /W4)
else()
    target_compile_options(servo_tool PRIVATE -Wall -Wextra)
endif()
