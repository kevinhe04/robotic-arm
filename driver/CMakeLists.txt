# Oldest CMake that can build this project; always the first line.
cmake_minimum_required(VERSION 3.15)
# Name the project and say it is written in C++ (CXX).
project(sts3215_arm CXX)

# Compile as C++17, and fail rather than silently fall back to an older standard.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # lets editors understand the code
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)       # optimized unless asked otherwise
endif()
# Turn on compiler warnings for every target. MSVC (Windows) spells the flags differently.
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

# Make a library called driver_core from these files
add_library(driver_core STATIC src/sts3215.cpp src/recording.cpp)
# Anything that uses driver_core can #include headers from src/.
target_include_directories(driver_core PUBLIC src)

# Pick the serial port file for this OS and store its name in the variable SERIAL_PORT.
if(WIN32)
    set(SERIAL_PORT src/serial_port_win32.cpp)
else()
    set(SERIAL_PORT src/serial_port_posix.cpp)
endif()
# Make a program called servo_tool from these source files...
add_executable(servo_tool src/main.cpp src/record_replay.cpp ${SERIAL_PORT})
# ...and link in the driver_core library (the protocol and recording code).
target_link_libraries(servo_tool PRIVATE driver_core)

# Tests: a second program, built from the test files plus the same library.
enable_testing()
add_executable(driver_tests tests/test_bus.cpp tests/test_tools.cpp)
target_include_directories(driver_tests PRIVATE third_party)   # where doctest.h lives
target_link_libraries(driver_tests PRIVATE driver_core)
# Register it with ctest, so `ctest --test-dir build` runs it.
add_test(NAME driver_tests COMMAND driver_tests)
