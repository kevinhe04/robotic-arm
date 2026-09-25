#!/usr/bin/env bash
# Builds servo_tool with the system C++ compiler (Apple clang on macOS). Output lands in build/.
# The macOS/Linux counterpart of build.bat, for machines without CMake.
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build
"${CXX:-c++}" -std=c++17 -Wall -Wextra -O2 \
    src/main.cpp src/sts3215.cpp src/serial_port_posix.cpp \
    -o build/servo_tool
echo "Built build/servo_tool"
