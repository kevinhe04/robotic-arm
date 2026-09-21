"""Reading the tablet screen and deciding what to press.

The only Python in this project. Everything else -- the servo bus, kinematics,
trajectories, safety and the application loop -- is C++ under ``driver/``.

Python earns its place here and nowhere else: the VLM call is HTTPS plus JSON
parsing, which is neither interesting C++ nor interesting robotics. If LeRobot
imitation learning ever happens, it lands here too, for the same reason.
"""
