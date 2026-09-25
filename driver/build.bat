@echo off
REM Builds servo_tool.exe with g++ (MSYS2 MinGW). Output lands in build\.
setlocal
set GXX=C:\msys64\mingw64\bin\g++.exe
if not exist build mkdir build
"%GXX%" -std=c++17 -Wall -Wextra -O2 -static ^
    src\main.cpp src\serial_port_win32.cpp src\sts3215.cpp ^
    -o build\servo_tool.exe
if errorlevel 1 (
    echo BUILD FAILED
    exit /b 1
)
echo Built build\servo_tool.exe
