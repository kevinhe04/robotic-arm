// serial_port.h -- thin wrapper around a USB serial port.
//
// The servos talk over a single half-duplex TTL serial line. The USB adapter
// (e.g. a Waveshare bus-servo board) shows up as an ordinary serial port --
// /dev/cu.usbmodem* on macOS, COM5 and so on on Windows -- and handles the
// direction switching for us, so from C++ this is just a normal serial port:
// write bytes, read bytes back.
//
// One interface, two implementations, picked at build time:
//   serial_port_posix.cpp   macOS / Linux (termios)
//   serial_port_win32.cpp   Windows. The only file that includes windows.h.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

class SerialPort {
public:
    SerialPort() = default;
    ~SerialPort();

    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    // port_name is "/dev/cu.usbmodem..." on macOS, "COM5" on Windows. Returns false and fills last_error().
    bool open(const std::string& port_name, uint32_t baud_rate);
    void close();
#ifdef _WIN32
    bool is_open() const { return handle_ != nullptr; }
#else
    bool is_open() const { return fd_ >= 0; }
#endif

    // Returns true only if every byte went out.
    bool write(const uint8_t* data, size_t length);

    // Reads up to `length` bytes, giving up after read_timeout_ms of silence.
    // Returns how many bytes actually arrived.
    size_t read(uint8_t* buffer, size_t length);

    // Throws away anything sitting in the driver's buffers. Worth doing before
    // each command so a late reply from the previous one is not mistaken for
    // the answer to this one.
    void flush();

    const std::string& last_error() const { return last_error_; }

private:
#ifdef _WIN32
    void* handle_ = nullptr;   // HANDLE, kept as void* so windows.h stays out of this header
#else
    int fd_ = -1;              // file descriptor, -1 when closed
#endif
    std::string last_error_;
};
