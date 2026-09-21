// serial_port.h -- thin wrapper around a Windows COM port.
//
// The servos talk over a single half-duplex TTL serial line. The USB adapter
// (e.g. a Waveshare bus-servo board) shows up on Windows as a COM port and
// handles the direction switching for us, so from C++ this is just a normal
// serial port: write bytes, read bytes back.
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

    // port_name is "COM5" and so on. Returns false and fills last_error().
    bool open(const std::string& port_name, uint32_t baud_rate);
    void close();
    bool is_open() const { return handle_ != nullptr; }

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
    void* handle_ = nullptr;   // HANDLE, kept as void* so windows.h stays out of this header
    std::string last_error_;
};
