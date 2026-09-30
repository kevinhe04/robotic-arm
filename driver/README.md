# Servo driver (C++)

The hand-written driver for the arm's Feetech STS3215 bus: the packet protocol, the
serial transport, and a `servo_tool` CLI for bringing the arm up from the desk.

This is the robot. Kinematics, trajectories, the safety layer, the control loop and the
Duolingo application loop all land here too. The only thing that does not is the VLM call
that reads the screen, which is Python in [`../perception/`](../perception/); see the
[root README](../README.md) for the arm as a whole.

Written from the protocol datasheet rather than using the SO-101 / TheRobotStudio
Python stack, on purpose — see the build-it-vs-import-it rule in [`../CLAUDE.md`](../CLAUDE.md).

First job: give each servo its own ID.

## Why IDs matter

The STS3215 is a *serial bus* servo. Unlike a hobby RC servo (one signal wire
each, PWM pulses), all four of these share the same three wires: power, ground,
and a single data line. They are daisy-chained, so the controller has one cable
going out to the whole arm.

Because they all hear every message, each servo needs a unique name — its ID.
A message says "servo 3, go to position 2048" and only servo 3 acts on it.

**Every STS3215 leaves the factory as ID 1.** Four servos straight out of the
box all answer to the same name, so nothing works until you rename them. That
is what this tool does.

## Hardware you need

- **The servos** — 4x Feetech STS3215.
- **A USB-to-serial bus adapter** — e.g. the Waveshare Serial Bus Servo Driver
  board or a Feetech FE-URT-1. This is the piece that converts USB into the
  one-wire half-duplex signal the servos speak. A plain FTDI/CH340 USB-serial
  cable will not work on its own: the servos transmit and receive on the *same*
  wire, and the adapter is what switches direction between the two.
- **A 12 V power supply** into the adapter board. The servos draw far more
  current than USB can give; USB only carries the data.

Plug the adapter in and it appears as a serial port: a `/dev/cu.usbmodem…` device on
macOS, a COM port on Windows. You normally don't need its name — every tool finds the
adapter by itself when exactly one USB serial adapter is plugged in, and refuses (listing
what it found) when there are none or several.

To pick one yourself, put it first: `build/servo_tool /dev/cu.usbmodem1101 scan`. On
macOS use the `cu.` device, not `tty.`: opening `tty.` waits for a carrier-detect signal
the adapter never sends.

## Build

On macOS, with the Xcode command line tools' clang:

```
./build.sh
```

or with CMake, on any platform:

```
cmake -S . -B build
cmake --build build
```

Either way you get `build/servo_tool`. On Windows, `build.bat` does the same with
MSYS2 g++ and produces `build\servo_tool.exe`.

## Assigning the IDs

Connect **one servo at a time**. Two factory-fresh servos on the bus both
answer to ID 1, and renaming would be a coin flip between them.

```
build/servo_tool assign 4
```

The tool prompts you before each servo, scans the bus, confirms exactly one
servo is connected, and renames it. Work through them in the order you will
mount them — base, shoulder, elbow, wrist — so the IDs match the joints.

When it finishes, chain all four back together and confirm:

```
build/servo_tool scan
```

You should see `found 4 servo(s): 1 2 3 4`.

## Other commands

```
build/servo_tool scan                list every servo answering
build/servo_tool ping 1              check one servo
build/servo_tool setid 1 3           rename servo 1 to 3, no prompts
build/servo_tool pos 3               read present position (0-4095)
```

Add `--baud 115200` before the command if a servo is not on the factory
1,000,000 baud.

## Record and replay

Teach the arm a motion by moving it by hand, then have it repeat it under power:

```
build/servo_tool record --seconds 15   torque off; move the arm by hand for 15 s
build/servo_tool replay                repeat it at the speed you moved it
build/servo_tool replay --seconds 8    ...or make the motion take 8 s
build/servo_tool replay --reverse      play it end to start
build/servo_tool torque-off        all joints limp -- support the arm first
```

`record` samples all four joints at up to 50 Hz into `recording.txt` (`--out` to change
it); Ctrl+C ends it early and still saves. Start and finish in the same resting pose.

`replay` follows the recording's own timing in a 25 Hz loop, sending each joint the
interpolated goal for "now" and checking where it actually is. It only goes where the
recording went:

- goals are clamped to each joint's recorded range
- it refuses to start unless the arm is within ~13° of the first pose, then closes that
  gap with a slow one-second approach
- `--seconds` too short for the servos' speed cap is refused up front, with the shortest
  value that would work
- a recording that ends away from where it started is retraced back, so torque goes off
  in the resting pose it began from
- a joint that falls behind, a bus error or Ctrl+C makes the arm **hold** its pose rather
  than go limp, because a limp shoulder drops the arm

Both write goal positions directly and bypass the safety layer, which does not exist
yet. They are bench tools, not for anything that runs unattended.

## How the protocol works

Each message is a packet on the serial line:

```
0xFF 0xFF   ID   LEN   INSTRUCTION   PARAMS...   CHECKSUM
```

- `0xFF 0xFF` marks the start of a packet.
- `ID` is who it is for. `0xFE` is broadcast — everyone obeys, nobody replies.
- `LEN` is how many bytes follow it (params + 2).
- `INSTRUCTION` is PING (0x01), READ (0x02) or WRITE (0x03).
- `CHECKSUM` is `~(ID + LEN + INSTRUCTION + params)`, low byte only, so the
  servo can tell a corrupted packet from a good one.

The servo replies in the same shape, with an error-flags byte where the
instruction was.

A servo's settings live in a numbered table inside it. Writing to address 5
changes its ID, address 56 reads back its position, and so on. Addresses below
40 are EEPROM (survive power-off) and are protected by a lock at address 55 —
so changing an ID is three steps: unlock, write the new ID, lock again.

## Tests

```
./build.sh test
```

No port and no servo needed. `Bus` depends on `Transport`, not on `SerialPort`, so the
tests hand it a fake that records every packet and replays scripted servo replies.

The bottom of `test_bus.cpp` holds tests marked `doctest::should_fail`: behaviour the
driver should have and does not yet. They count as passing while they fail. When a fix
makes one pass, the run goes red with "should have failed but didn't" — delete the
marker and it stays green from then on.

## Files

| File | What it does |
| --- | --- |
| `src/transport.h` | The bytes-in/bytes-out interface `Bus` depends on |
| `src/serial_port.h` | Serial port interface, plus finding the adapter automatically |
| `src/serial_port_posix.cpp` | macOS / Linux implementation, on termios |
| `src/serial_port_win32.cpp` | Windows implementation, on the Win32 COM API |
| `src/sts3215.h/.cpp` | Builds and parses servo packets; ping, read, write, set ID |
| `src/main.cpp` | `servo_tool`: IDs, register reads/writes, `torque-off`, dispatch |
| `src/recording.h/.cpp` | A recorded motion: file format, interpolation, reverse/retrace, timing |
| `src/record_replay.cpp` | The `record` and `replay` commands |
| `tests/fake_serial_port.h` | A fake `Transport` that scripts servo replies, with or without echo |
| `tests/test_bus.cpp` | Bus behaviour against both adapter styles, then the hardening backlog |
| `tests/test_tools.cpp` | The replay plan, the recording file format, port auto-detect |

Everything above the serial port is platform-independent; the build picks one
`serial_port_*.cpp`. Keeping the OS behind that one interface is deliberate: if the
control loop ever moves onto a microcontroller, a third implementation of that file is
the whole port, not a rewrite of the driver.

## Next

Build step 2 of the [roadmap](../README.md#roadmap):

- Move a servo to a position and read it back
- Read all four positions at once, so I can record arm poses by hand
- Sync-write, to command all four joints in a single packet

Alongside that, the parts of the design worth fixing while the code is still small:
split packet building from the serial transaction so the protocol is testable without a
port; give registers a size so a 2-byte read cannot be issued as a 1-byte one; return a
typed error instead of `bool` + `last_error_`, so "no reply" and "servo reported a fault"
stop looking identical; re-lock EEPROM from a destructor rather than on every return path;
and stop scanning all 254 IDs when the arm has four.
