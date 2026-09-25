// serial_port_posix.cpp -- SerialPort for macOS and Linux, on termios.
//
// Same behaviour as the Win32 version: raw 8N1, no flow control, and a read
// that gives up after 50 ms with nothing arriving or 20 ms of silence mid-reply.
// Windows gets those timeouts from COMMTIMEOUTS; here they are built from poll().
#include "serial_port.h"

#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#ifdef __APPLE__
#include <IOKit/serial/ioss.h>  // IOSSIOSPEED
#endif

namespace {

constexpr int kFirstByteTimeoutMs = 50;  // matches ReadTotalTimeoutConstant
constexpr int kInterByteTimeoutMs = 20;  // matches ReadIntervalTimeout
constexpr int kWriteTimeoutMs = 100;     // matches WriteTotalTimeoutConstant

std::string errno_message(const std::string& prefix) {
    return prefix + " (errno " + std::to_string(errno) + "): " + std::strerror(errno);
}

// Waits until fd is readable (POLLIN) or writable (POLLOUT). False on timeout.
bool wait_for(int fd, short events, int timeout_ms) {
    pollfd p{fd, events, 0};
    int ready;
    do {
        ready = poll(&p, 1, timeout_ms);
    } while (ready < 0 && errno == EINTR);
    return ready > 0;
}

#ifndef __APPLE__
// Linux has fixed Bxxx constants rather than accepting an arbitrary rate.
bool to_speed_constant(uint32_t baud_rate, speed_t* out) {
    switch (baud_rate) {
        case 9600: *out = B9600; return true;
        case 38400: *out = B38400; return true;
        case 57600: *out = B57600; return true;
        case 115200: *out = B115200; return true;
        case 500000: *out = B500000; return true;
        case 1000000: *out = B1000000; return true;
        default: return false;
    }
}
#endif

}  // namespace

SerialPort::~SerialPort() { close(); }

bool SerialPort::open(const std::string& port_name, uint32_t baud_rate) {
    close();

    // O_NONBLOCK so open() does not hang waiting for a carrier-detect line the
    // adapter does not have. It stays non-blocking; read/write wait with poll().
    // On macOS use the /dev/cu.* device, not /dev/tty.*, for the same reason.
    const int fd = ::open(port_name.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        last_error_ = errno_message("cannot open " + port_name);
        return false;
    }

    // No sharing, we want the port to ourselves.
    if (ioctl(fd, TIOCEXCL) < 0) {
        last_error_ = errno_message("cannot get exclusive access to " + port_name);
        ::close(fd);
        return false;
    }

    termios tio{};
    if (tcgetattr(fd, &tio) < 0) {
        last_error_ = errno_message("tcgetattr failed");
        ::close(fd);
        return false;
    }

    // Raw bytes: no line editing, no echo, no translating 0x0D or 0x11/0x13.
    cfmakeraw(&tio);
    tio.c_cflag &= ~(CSIZE | PARENB | CSTOPB | CRTSCTS | HUPCL);
    tio.c_cflag |= CS8 | CLOCAL | CREAD;  // 8N1, ignore modem control lines
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;

#ifdef __APPLE__
    // Placeholder speed; the real one is set with IOSSIOSPEED after tcsetattr,
    // because macOS termios only accepts the classic rates and 1 Mbaud is not one.
    cfsetspeed(&tio, B9600);
#else
    speed_t speed;
    if (!to_speed_constant(baud_rate, &speed)) {
        last_error_ = "baud rate " + std::to_string(baud_rate) + " not supported";
        ::close(fd);
        return false;
    }
    cfsetspeed(&tio, speed);
#endif

    if (tcsetattr(fd, TCSANOW, &tio) < 0) {
        last_error_ = errno_message("tcsetattr failed");
        ::close(fd);
        return false;
    }

#ifdef __APPLE__
    speed_t speed = baud_rate;
    if (ioctl(fd, IOSSIOSPEED, &speed) < 0) {
        last_error_ = errno_message("cannot set baud rate " + std::to_string(baud_rate));
        ::close(fd);
        return false;
    }
#endif

    // Hold DTR and RTS low, as the Win32 version does. Best effort: some
    // adapters do not implement modem lines at all.
    int lines = TIOCM_DTR | TIOCM_RTS;
    ioctl(fd, TIOCMBIC, &lines);

    fd_ = fd;
    flush();
    last_error_.clear();
    return true;
}

void SerialPort::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool SerialPort::write(const uint8_t* data, size_t length) {
    if (fd_ < 0) {
        last_error_ = "port is not open";
        return false;
    }
    size_t sent = 0;
    while (sent < length) {
        const ssize_t n = ::write(fd_, data + sent, length - sent);
        if (n > 0) {
            sent += static_cast<size_t>(n);
            continue;
        }
        if (n < 0 && errno != EAGAIN && errno != EINTR) {
            last_error_ = errno_message("write failed");
            return false;
        }
        if (!wait_for(fd_, POLLOUT, kWriteTimeoutMs)) {
            last_error_ = "short write";
            return false;
        }
    }
    return true;
}

size_t SerialPort::read(uint8_t* buffer, size_t length) {
    if (fd_ < 0) {
        last_error_ = "port is not open";
        return 0;
    }
    size_t got = 0;
    int timeout_ms = kFirstByteTimeoutMs;
    while (got < length) {
        if (!wait_for(fd_, POLLIN, timeout_ms)) {
            break;  // quiet for long enough -- whatever arrived is the answer
        }
        const ssize_t n = ::read(fd_, buffer + got, length - got);
        if (n > 0) {
            got += static_cast<size_t>(n);
            timeout_ms = kInterByteTimeoutMs;
        } else if (n == 0 || (errno != EAGAIN && errno != EINTR)) {
            last_error_ = (n == 0) ? "port closed" : errno_message("read failed");
            break;
        }
    }
    return got;
}

void SerialPort::flush() {
    if (fd_ >= 0) {
        tcflush(fd_, TCIOFLUSH);
    }
}
