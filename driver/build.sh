#!/usr/bin/env bash
# ./build.sh builds build/servo_tool and build/driver_tests; ./build.sh test also runs the tests.
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build
CXX=${CXX:-c++}
FLAGS=(-std=c++17 -Wall -Wextra -O2 -Isrc)
CORE=(src/sts3215.cpp src/recording.cpp)

"$CXX" "${FLAGS[@]}" src/main.cpp src/record_replay.cpp src/serial_port_posix.cpp "${CORE[@]}" \
    -o build/servo_tool
"$CXX" "${FLAGS[@]}" -Ithird_party tests/test_bus.cpp tests/test_tools.cpp "${CORE[@]}" \
    -o build/driver_tests
echo "Built build/servo_tool and build/driver_tests"

if [[ ${1:-} == test ]]; then build/driver_tests; fi
