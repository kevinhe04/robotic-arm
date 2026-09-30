@echo off
REM Builds build\servo_tool.exe with MSYS2 g++.
set GXX=C:\msys64\mingw64\bin\g++.exe
if not exist build mkdir build
"%GXX%" -std=c++17 -Wall -Wextra -O2 -static -Isrc ^
    src\main.cpp src\record_replay.cpp src\recording.cpp src\sts3215.cpp src\serial_port_win32.cpp ^
    -o build\servo_tool.exe || (echo BUILD FAILED & exit /b 1)
echo Built build\servo_tool.exe
