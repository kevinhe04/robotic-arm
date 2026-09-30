# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

A 4-DOF desktop robot arm, 3D-printed from the open-source [SO-101](https://github.com/TheRobotStudio/SO-ARM100)
design, that operates a touchscreen — the flagship demo is the arm solving Duolingo lessons on a
tablet by itself.

Three goals shape every decision here:

1. **Learning robotics.** The owner is breaking into the field. The point is to understand the
   stack, not to ship fastest.
2. **Learning C++.** This is an explicit deliverable, not a side effect — see below.
3. **Public portfolio.** This repo is read by recruiters at SF robotics startups. Directory
   layout, naming, README and commit history are part of the deliverable.

## The build-it-vs-import-it rule

The most important convention here. An off-the-shelf library ([LeRobot](https://github.com/huggingface/lerobot))
can already drive this arm end to end. We deliberately do not use it for the core control path,
because reimplementing that path is the learning goal. Do not "helpfully" replace hand-written
code with a library call.

**Written by hand — never swap for a dependency:**

| Area | Why it stays hand-written |
| --- | --- |
| Feetech bus driver | Register/control-table level understanding of serial servos |
| Kinematics | FK, closed-form IK, Jacobian. The core robotics skill |
| Trajectory generation | Velocity profiles, fixed-rate control loop |
| Calibration | Joint zeroing, and the screen↔robot transform no library provides |
| Safety layer | Joint/velocity/temperature limits, e-stop |

All of it lives in `driver/`, in C++.

**Use the library — do not reinvent:**

- Camera intrinsics, ArUco, homography solving → OpenCV (the C++ API)
- Reading the screen (OCR / question understanding) → a VLM, not custom CV
- Imitation-learning infrastructure (ACT/diffusion policy, dataset format, training) → LeRobot

When unsure which side something falls on, ask rather than assume.

## Languages, and where the line is

**This is a C++ project.** `driver/` owns the packet protocol, serial transport, register map,
kinematics, trajectory generation, safety, the control loop and the application loop. Default to
C++ for anything new.

**Python appears exactly once**, in `perception/`: the VLM call that reads the tablet screen.
That is HTTPS plus JSON parsing — neither interesting C++ nor interesting robotics. LeRobot
imitation learning, if it happens, lands there too. Nothing else goes in Python.

Three things follow, and all three have been got wrong before:

- **Do not propose moving work into Python.** The servo driver was written in Python first and
  deliberately replaced. That decision is made; re-proposing Python for kinematics, calibration
  or motion is re-treading it.
- **OpenCV is not a reason to reach for Python.** It is a C++ library — `cv::calibrateCamera`,
  `cv::findHomography`, `cv::aruco` are directly available. Camera→screen calibration stays in
  `driver/`. Setting OpenCV up on Windows via vcpkg or MSYS2 is a real one-time cost, but that
  is a scheduling problem, not a language argument.
- **Do not propose an ESP32 or any microcontroller.** Servo ID assignment, register reads and
  position writes are desk tasks needing a USB-TTL adapter and a servo — an MCU adds a firmware
  target and a host link for nothing. The question only becomes live at build step 5 (fixed-rate
  control loop), where USB-serial latency is the real argument, and it is optional even then.
  The cheap hedge is already in place: the OS lives only behind `serial_port.h`, with one
  implementation per platform (`serial_port_posix.cpp`, `serial_port_win32.cpp` — the only file
  that includes `windows.h`). An MCU would be one more implementation. **Keep it that way.**

## Writing C++ here

The owner's rule is *maximize C++ learning, but reject anything out of the way*.

**Do not write the interesting C++ for him.** Plumbing, build files, docs and scaffolding are
fair game. Protocol design, kinematics, trajectory generation and the control loop are the
deliverable — explain, review, suggest an approach, but let him write them. If asked directly to
implement one of those, say so and offer to review instead.

The driver as it stands is a first pass with known weaknesses. These are his to fix, and are
listed here so they are not silently "helpfully" fixed:

- Packet building and the serial transaction are fused in `Bus::transact`, which is why nothing
  is testable without a COM port. Split them.
- Registers are bare `uint8_t` addresses with no size, so `read_u8` on a 2-byte register compiles
  and silently returns the low byte. A `struct Register { address; size; }` makes that
  unrepresentable.
- `bool` + `last_error_` cannot distinguish "no reply" from "servo reported a fault" — one should
  be retried and the other must not be. Needs a typed error.
- `set_id` leaves EEPROM unlocked if the ID write fails. RAII: re-lock from a destructor.
- The retry loop re-reads the port but never re-sends, so a corrupted transmit is unrecoverable.
- `scan()` pings all 254 IDs on a 4-servo arm.
- Found by the test harness: a stray `0xFF` before a reply header makes the frame search `break`
  instead of `continue`, losing the real reply one byte later.
- Found by the test harness: on an echoing adapter, silence is reported as "malformed reply"
  because the buffer holds the echo.

Each of these has a `should_fail` test at the bottom of `driver/tests/test_bus.cpp`.

**`record` / `replay` were written by Claude at the owner's request**, as an explicit exception to
the rule above -- he chose to review them rather than write them. They are bench tools: `replay`
writes goal positions directly, because no safety layer exists yet. When the safety layer lands,
move `replay` onto it; do not treat `record_replay.cpp` as the model for how motion code should look.

## Hardware

- **Arm:** SO-101 printed parts, built as a **4-DOF subset** — the stock follower is 6-DOF/6 servos,
  and this build omits `wrist_roll` and the gripper. A rigid capacitive stylus replaces the gripper.
- **Servos:** 4× Feetech STS3215 on a shared half-duplex TTL bus, daisy-chained, 1 Mbaud default.
- **Servo IDs** (assign base-outward; IDs live in servo EEPROM, so this is one-time per motor):

  | ID | Joint | Axis |
  | --- | --- | --- |
  | 1 | `base_pan` | yaw |
  | 2 | `shoulder_lift` | pitch |
  | 3 | `elbow_flex` | pitch |
  | 4 | `wrist_flex` | pitch (sets stylus angle) |

- **Kinematic consequence:** joints 2–4 form a planar 3R arm on a yaw turntable. Stylus tip
  (x, y, z) plus tool pitch is exactly 4 constraints for 4 joints — IK is closed-form and uniquely
  determined. Solve it analytically; do not reach for a numerical solver.

**Because this is a 4-servo build, LeRobot's `so101_follower` robot type will not work as-is** — it
expects 6 motors. Any LeRobot interop needs a custom robot class.

## Working with hardware safely

The arm is real, has torque, and is usually pointed at a tablet. When writing or running code that
touches the bus:

- **Never run a motion command as a side effect of another task.** Confirm with the user first.
- Default to torque disabled. Enable torque explicitly, in the narrowest scope possible.
- Every new motion path goes through the safety layer — no direct `goal_position` writes from
  application code.
- Test new kinematics against a dry-run backend before the physical arm.
- Assume the arm may be holding a stylus 2mm above a screen. Z-axis bugs scratch hardware.

## Layout

```
driver/         # C++ — the robot
  src/
    sts3215.*             # packet protocol + bus transport
    transport.h           # bytes in/out interface Bus depends on (SerialPort, test fake)
    serial_port.h         # serial port + adapter auto-detect; the only place the OS leaks in
    serial_port_posix.cpp # macOS / Linux. 1 Mbaud on macOS needs IOSSIOSPEED
    serial_port_win32.cpp # Windows. The only file that includes windows.h -- keep it so
    recording.*           # recorded motion: file format, interpolation, reverse/retrace, timing
    record_replay.cpp     # `servo_tool record` / `servo_tool replay` bench tools
    main.cpp              # servo_tool CLI
                          # kinematics, motion, safety, calibration, apps land here
  tests/          # doctest unit tests against a fake serial port -- no hardware needed
  third_party/    # doctest.h, vendored single header
  CMakeLists.txt, build.sh (macOS), build.bat (Windows)
perception/     # Python — the VLM call, and nothing else
configs/        # servos.yaml: ids, limits, calibration output
hardware/       # BOM, print settings, wiring, deviations from stock SO-101
```

Keep the bus layer free of kinematics — it speaks only in servo IDs and encoder counts — and keep
`perception/` free of motion code. The layering is part of what a reader is evaluating.

**Open question, not yet decided:** nothing currently reads `configs/servos.yaml`. Joint IDs could
become a `constexpr` table compiled into `driver/`, but calibration output (offsets, limits) is
written at runtime and has to live in a file, so some runtime config format is still needed.
yaml-cpp, nlohmann/json, or a hand-rolled parser are all defensible. Ask before picking one.

## Commands

```
cd driver && ./build.sh          # Apple clang; or cmake -S . -B build && cmake --build build
driver/build/servo_tool scan       # port auto-detected when one USB adapter is plugged in
```

Development machine is a MacBook: Apple clang 17, **no CMake installed** — use `build.sh`.
The Waveshare adapter enumerates as `/dev/cu.usbmodem5B8E1134991`. `build.bat` (MSYS2 g++,
hardcoded path) is kept so the Windows backend still builds, but is not the primary path.

`perception/` has no Python environment yet — no `uv`, no `.venv`, no `pytest`. It also has no
code in it, so this is not blocking anything.

## Testing

**Tests exist; the driver hardening they describe is not done yet.** `driver/tests/`, doctest,
run with `cd driver && ./build.sh test` (or `ctest` after a CMake build). `Bus` talks through the
`Transport` interface (`src/transport.h`); `SerialPort` is the real implementation and
`tests/fake_serial_port.h` is a fake that scripts servo replies, with or without adapter echo.

- `test_bus.cpp` pins down current bus behaviour, every case against both adapter styles. Its
  bottom section is the hardening to-do list as `doctest::should_fail` tests: they pass *because*
  they fail, and when a fix makes one pass the marker comes off. The skipped placeholders need
  an API he has not designed yet -- do not design it for him, and do not "fix" a should_fail
  test by weakening its assertion.
- `test_tools.cpp` covers the recording/replay plan and serial-port auto-detect.

Once kinematics lands, assert `FK(IK(pose)) ≈ pose` over randomized reachable poses, and assert IK
rejects out-of-envelope targets rather than returning something plausible-looking.

## Build order

All seven steps are C++ in `driver/`. Step 7 calls out to `perception/` for the VLM.

1. Assign servo IDs over the bus, one motor at a time (write to EEPROM). **Written** —
   `servo_tool assign`. Compiles clean, has never run against a physical servo.
2. Read/write position and torque on a single servo; then sync-read all four. `SYNC_READ`/
   `SYNC_WRITE` are not implemented yet. Worth fixing the driver weaknesses listed above while
   the code is still small.
3. Joint calibration — zero offsets, direction signs, software limits → `configs/`.
4. FK, then closed-form IK, with tests, before any real motion.
5. Trajectory profiles + fixed-rate control loop.
6. Camera → screen → robot-base calibration (homography + touch-plane fit).
7. Duolingo application loop on top.

Untested-on-hardware caveat: no code here has yet talked to a physical STS3215, and there is no
test suite. Treat the first real run as debugging, and expect adapter-specific surprises around
echo and timing.

## History

An earlier repo (`kevinhe04/robotic-arm`) held a Python implementation of the same servo driver,
written first and deliberately replaced by the C++ here. It is not a dependency and not a
fallback. Do not suggest porting from it or reviving it.
