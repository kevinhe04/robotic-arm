#!/usr/bin/env bash
# torque_off.sh -- disable torque on all four joints.
#
#   scripts/torque_off.sh /dev/cu.usbmodem1101
#
# Every joint goes limp at once, so SUPPORT THE ARM before running this.
# Not an e-stop: it needs a working bus. The real e-stop is the 12V supply switch.
set -uo pipefail

if [[ $# -lt 1 ]]; then
    echo "usage: torque_off.sh <PORT>"
    exit 1
fi

TOOL="$(cd "$(dirname "$0")/.." && pwd)/build/servo_tool"
result=0
for id in 1 2 3 4; do
    "$TOOL" "$1" write "$id" 40 0 || result=1
done
exit $result
