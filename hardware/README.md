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
| Feetech STS3215 bus servo | 4 | 7.4V; check gear ratio against the SO-101 BOM per joint |
| Printed SO-101 structural parts | 1 set | Upstream STLs, 4-DOF subset |
| USB-TTL bus adapter | 1 | Feetech URT-1 or Waveshare equivalent |
| Power supply | 1 | Servos do not run on USB power alone |
| Capacitive stylus | 1 | Rigid mount in place of the gripper |
| Tablet | 1 | Demo target |

<!-- TODO: fill in exact part numbers, print settings (material, layer height, infill),
     and wiring photos once the build is assembled. -->

## Bus wiring

All four servos daisy-chain onto a single half-duplex TTL line at 1 Mbaud. The bus is
request/response — only one transaction is in flight at a time, which is why
`sts3215::Bus` in `driver/` is the serialization point for the whole robot.
