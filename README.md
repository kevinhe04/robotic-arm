# Touchscreen Arm

A 4-DOF desktop robot arm that operates a tablet — reading the screen, deciding what to
press, and pressing it. The demo application is solving Duolingo lessons unassisted.

> **Status:** early. Build step 1 is done and verified against real hardware — four servos
> assigned, chained, and answering. Nothing has moved under power yet. See
> [Roadmap](#roadmap).

<!-- media/demo.gif goes here once the arm moves -->

## Why it's built this way

An off-the-shelf library ([LeRobot](https://github.com/huggingface/lerobot)) can already
drive this hardware. This repo deliberately does not use it for the core control path,
because writing that path is the point.

**Hand-written here:** the Feetech serial protocol, the servo bus driver, forward and
inverse kinematics, trajectory generation, calibration, and the safety layer.

**Imported, not reinvented:** OpenCV for camera intrinsics and homography, a VLM for
reading the screen, and LeRobot for imitation-learning infrastructure if and when the
project gets there.

## C++, with one Python exception

The robot is **C++**, in [`driver/`](driver/): packet protocol, serial transport,
kinematics, trajectory generation, safety, the control loop, and the application loop.

**Python appears exactly once**, in [`perception/`](perception/), for the VLM call that
reads the screen. That is HTTPS and JSON parsing — neither interesting C++ nor interesting
robotics, and the one place where another language buys something real.

OpenCV is *not* a reason to use Python: it is a C++ library, so `cv::findHomography` and
`cv::aruco` are available directly and the camera→screen calibration stays in `driver/`.

## Hardware

Printed from the open-source [SO-101](https://github.com/TheRobotStudio/SO-ARM100)
design, built as a **4-DOF subset** — the stock follower arm is 6-DOF, and this build
drops `wrist_roll` and the gripper in favour of a rigid capacitive stylus.

| Servo ID | Joint | Axis |
| --- | --- | --- |
| 1 | `base_pan` | yaw |
| 2 | `shoulder_lift` | pitch |
| 3 | `elbow_flex` | pitch |
| 4 | `wrist_flex` | pitch (stylus angle) |

Four [Feetech STS3215](https://www.feetechrc.com/) bus servos daisy-chained on one
half-duplex TTL line at 1 Mbaud.

The reduced DOF count is a deliberate fit to the task, not just a parts constraint.
Joints 2–4 form a planar 3R arm on a yaw turntable, so stylus tip `(x, y, z)` plus tool
pitch is exactly four constraints for four joints — **the inverse kinematics are
closed-form and uniquely determined**, with no redundancy to resolve numerically.

Wiring, BOM and deviations from stock SO-101 are in [`hardware/`](hardware/README.md).

## Quickstart

```bash
cd driver
./build.sh                                     # or: cmake -S . -B build && cmake --build build
```

With the arm powered and plugged in — assign IDs one servo at a time, then confirm the
chain:

```
build/servo_tool /dev/cu.usbmodem1101 assign 4     # ls /dev/cu.* to find your adapter
build/servo_tool /dev/cu.usbmodem1101 scan
```

Every STS3215 ships as ID 1, so a freshly assembled arm has four servos answering to the
same address. `assign` walks the chain base-outward and refuses to write whenever more
than one servo is connected, rather than silently reassigning the wrong joint.

## Layout

```
driver/         # C++ — the robot
  src/
    sts3215.*     # packet protocol and bus transport
    serial_port.h           # serial port interface
    serial_port_posix.cpp   # macOS / Linux (termios)
    serial_port_win32.cpp   # Windows; the only file that includes windows.h
    main.cpp      # servo_tool CLI
  scripts/        # bench bring-up: first_motion.sh, torque_off.sh
  CMakeLists.txt, build.sh, build.bat
perception/     # Python — the VLM call, and nothing else
configs/        # servos.yaml: ids, limits, calibration output
hardware/       # BOM, print settings, wiring, deviations from stock SO-101
```

The bus layer speaks only in servo IDs and encoder counts — it knows nothing about links
or joint angles. `perception/` holds no motion code. The layering is deliberate.

## Roadmap

- [x] **1.** Servo bus driver and ID assignment — *verified on hardware: all four IDs
      assigned, chained bus scans clean, EEPROM re-locks correctly*
- [ ] **2.** Read/write position and torque; sync-read all four joints
- [ ] **3.** Joint calibration — zero offsets, direction signs, software limits
- [ ] **4.** Forward kinematics, then closed-form IK, with round-trip tests
- [ ] **5.** Trajectory profiles and a fixed-rate control loop
- [ ] **6.** Camera → screen → robot-base calibration
- [ ] **7.** The Duolingo application loop

All seven are C++ in `driver/`. Step 7 calls out to `perception/` for the VLM.

## Testing

There is no test suite yet. That is the first thing to fix, before more driver features
land.

`Bus` already takes `SerialPort&` by reference, so a fake port behind that interface is
the whole job: it makes packet framing, echo handling and the EEPROM lock sequence
testable without a COM port or a servo. doctest or Catch2, single header, no build
complexity.

Two things worth covering from the start, because they are where a servo driver actually
breaks:

- **Both adapter styles.** Some USB-TTL adapters echo transmitted bytes back to the host
  and some do not. A driver that assumes one silently fails on the other, and you find
  out only when you are holding different hardware.
- **Round-tripping**, once kinematics lands: assert `FK(IK(pose)) ≈ pose` over randomized
  reachable poses, and assert IK rejects out-of-envelope targets rather than returning
  something plausible-looking.
