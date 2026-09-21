# Hardware notes

## Deviations from stock SO-101

The upstream [SO-101](https://github.com/TheRobotStudio/SO-ARM100) follower is 6-DOF and
uses six STS3215 servos. This build uses **four**:

- `wrist_roll` is omitted — a stylus is rotationally symmetric about its own axis, so
  the joint would add a degree of freedom the task cannot use.
- The gripper is omitted and replaced with a rigid capacitive stylus mount.

Consequence for tooling: LeRobot's `so101_follower` robot type expects six motors and
will not drive this arm without a custom robot class.

## Bill of materials

| Part | Qty | Notes |
| --- | --- | --- |
| Feetech STS3215 bus servo | 4 | **12V class** (see below); check gear ratio against the SO-101 BOM per joint |
| Printed SO-101 structural parts | 1 set | Upstream STLs, 4-DOF subset |
| USB-TTL bus adapter | 1 | Feetech URT-1 or Waveshare equivalent |
| Power supply | 1 | Servos do not run on USB power alone |
| Capacitive stylus | 1 | Rigid mount in place of the gripper |
| Tablet | 1 | Demo target |

<!-- TODO: fill in exact part numbers, print settings (material, layer height, infill),
     and wiring photos once the build is assembled. -->

## Measured on first bringup (2026-09-21)

All four servos, read off the bus with `servo_tool read`:

| Register | Value | Meaning |
| --- | --- | --- |
| 62 present voltage | 123–125 | **12.3–12.5 V** |
| 14 max voltage limit | 140 | 14.0 V ceiling |
| 15 min voltage limit | 40 | 4.0 V floor |
| 63 present temperature | 30–33 | °C, idle |
| 13 max temperature limit | 70 | °C |
| 40 torque enable | 0 | torque **off** at power-up |
| 55 EEPROM lock | 1 | locked, as it should be |

**These are 12V-class servos, not 7.4V.**

**Torque is off at power-up, and goal position defaults to 0.** Those two facts together
are a trap: the servos sit wherever they physically are while torque is off, but the
instant torque is enabled every joint drives to 0. On first bringup one servo was sitting
at 2046 (~180°), so enabling torque would have swung it half a revolution without warning.

Safe sequence for enabling torque on an assembled arm:

1. Read present position (register 56)
2. Write that value to goal position (register 42)
3. *Then* enable torque (register 40)

That way the servo's first act is to hold where it already is, rather than lunge somewhere.

Note that the horn cannot be turned by hand even with torque off — the STS3215 gearbox
has a high reduction ratio and is effectively non-backdrivable. That is mechanical, not
electrical, and is not a sign that torque is engaged.

## Bus wiring

All four servos daisy-chain onto a single half-duplex TTL line at 1 Mbaud. The bus is
request/response — only one transaction is in flight at a time, which is why
`sts3215::Bus` in `driver/` is the serialization point for the whole robot.
