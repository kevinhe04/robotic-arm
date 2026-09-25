#!/usr/bin/env bash
# first_motion.sh -- move ONE servo a small step and back, under power, for the first time.
#
#   scripts/first_motion.sh /dev/cu.usbmodem1101 3          step servo 3 by +100 counts and back
#   scripts/first_motion.sh /dev/cu.usbmodem1101 3 -150     step by -150 counts instead
#
# Bench bring-up only. It drives servo_tool's raw `write` command, so it bypasses the
# (not yet written) safety layer. Nothing in the application should move the arm this way.
#
# Order of operations, and why:
#   1. ping, and read temperature/voltage       -- is the servo alive and sane?
#   2. read the present position
#   3. set a low speed, low acceleration and a 50% torque limit (RAM, reset on power-off),
#      then write goal = present position BEFORE enabling torque, so the servo holds where
#      it is instead of jumping to whatever stale goal is in the register
#   4. enable torque, step by DELTA, read back, step home, read back
#   5. disable torque -- the joint goes limp, so be holding the arm
set -uo pipefail

if [[ $# -lt 2 ]]; then
    echo "usage: first_motion.sh <PORT> <ID> [DELTA]"
    echo "  PORT is /dev/cu.usbmodem... (ls /dev/cu.*)"
    echo "  DELTA is in encoder counts, default 100 (about 9 degrees), max 200."
    exit 1
fi

PORT=$1
ID=$2
DELTA=${3:-100}

TOOL="$(cd "$(dirname "$0")/.." && pwd)/build/servo_tool"
if [[ ! -x $TOOL ]]; then
    echo "servo_tool not found at $TOOL -- run ./build.sh first."
    exit 1
fi

if ! [[ $DELTA =~ ^-?[0-9]+$ ]]; then
    echo "DELTA must be an integer, got '$DELTA'"
    exit 1
fi
# Keep the first moves small. 4096 counts is one full turn, so 200 is about 18 degrees.
if (( DELTA < -200 || DELTA > 200 )); then
    echo "DELTA $DELTA is too big for a first move. Keep it within -200..200."
    exit 1
fi

# Goal speed in steps/s, acceleration in units of 100 steps/s^2, torque limit in 0.1%.
SPEED=300
ACCEL=20
TORQUE_LIMIT=500

TORQUE_ON=0

tool() { "$TOOL" "$PORT" "$@" | grep -v '^opened '; return "${PIPESTATUS[0]}"; }

fail() {
    echo
    echo "FAILED: $1"
    if (( TORQUE_ON )); then
        echo "trying to disable torque on servo $ID..."
        tool write "$ID" 40 0
    fi
    exit 1
}

# Ctrl+C mid-move: drop torque rather than leave the joint holding at speed.
trap 'fail "interrupted"' INT

confirm() {
    echo
    read -r -p "$1 Press Enter to continue, Ctrl+C to abort. "
}

echo
echo "=== first motion: servo $ID on $PORT, step $DELTA counts ==="
echo
echo "Before continuing:"
echo "  - stylus well clear of the tablet and anything else"
echo "  - one hand ready to support the arm; torque OFF at the end makes the joint go limp"
echo "  - the 12V supply switch within reach -- that is your e-stop"
confirm ""

echo
echo "[1/5] checking the servo"
tool ping "$ID" || fail "servo $ID did not answer"
tool read "$ID" 63 || fail "could not read temperature"
tool read "$ID" 62 || fail "could not read voltage"
echo "  (reg 63 is temperature in C, reg 62 is voltage in 0.1 V -- expect roughly 25-40 and 110-125)"

echo
echo "[2/5] reading present position"
START=$("$TOOL" "$PORT" pos "$ID" | awk '/position:/ {print $4}')
[[ -n $START ]] || fail "could not read position"
TARGET=$(( START + DELTA ))
echo "  start = $START, target = $TARGET"
if (( TARGET < 0 || TARGET > 4095 )); then
    fail "target $TARGET is outside 0-4095. Try the opposite sign for DELTA."
fi

echo
echo "[3/5] limiting speed, acceleration and torque; holding goal at the present position"
tool write "$ID" 46 "$SPEED" 2 || fail "could not set speed"
tool write "$ID" 41 "$ACCEL" || fail "could not set acceleration"
tool write "$ID" 48 "$TORQUE_LIMIT" 2 || fail "could not set torque limit"
tool write "$ID" 42 "$START" 2 || fail "could not set goal position"

confirm "About to ENABLE TORQUE on servo $ID. It should hold still, not move."
TORQUE_ON=1
tool write "$ID" 40 1 || fail "could not enable torque"

confirm "[4/5] Torque is on. Next: move servo $ID from $START to $TARGET, then back."
tool write "$ID" 42 "$TARGET" 2 || fail "move out failed"
sleep 2
tool pos "$ID"

tool write "$ID" 42 "$START" 2 || fail "move back failed"
sleep 2
tool pos "$ID"

confirm "[5/5] Support the arm now -- torque goes OFF and the joint will go limp."
tool write "$ID" 40 0 || fail "could not disable torque"
TORQUE_ON=0

echo
echo "Done. Servo $ID moved and came back. Compare the positions printed above with"
echo "$START and $TARGET: a few counts off is normal, tens of counts off means the joint"
echo "is loaded or binding."
