cmake_minimum_required(VERSION 3.15)
project(sts3215_arm CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

# No OS dependency: shared by the tool and the tests.
add_library(driver_core STATIC src/sts3215.cpp src/recording.cpp)
target_include_directories(driver_core PUBLIC src)

if(WIN32)
    set(SERIAL_PORT src/serial_port_win32.cpp)
else()
    set(SERIAL_PORT src/serial_port_posix.cpp)
endif()
add_executable(servo_tool src/main.cpp src/record_replay.cpp ${SERIAL_PORT})
target_link_libraries(servo_tool PRIVATE driver_core)

enable_testing()
add_executable(driver_tests tests/test_bus.cpp tests/test_tools.cpp)
target_include_directories(driver_tests PRIVATE third_party)
target_link_libraries(driver_tests PRIVATE driver_core)
add_test(NAME driver_tests COMMAND driver_tests)
