#include "serial_port.h"

#include <windows.h>

namespace {

std::string format_last_win_error(const std::string& prefix) {
    DWORD code = GetLastError();
    char* text = nullptr;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, code, 0, reinterpret_cast<char*>(&text), 0, nullptr);
    std::string message = prefix + " (error " + std::to_string(code) + ")";
    if (text) {
        std::string detail(text);
        while (!detail.empty() && (detail.back() == '\n' || detail.back() == '\r')) {
            detail.pop_back();
        }
        message += ": " + detail;
        LocalFree(text);
    }
    return message;
}

}  // namespace

std::vector<std::string> SerialPort::list_adapters() {
    // Windows has no naming pattern for USB adapters, so this is every COM port present.
    std::vector<std::string> found;
    char target[256];
    for (int n = 1; n <= 255; ++n) {
        const std::string name = "COM" + std::to_string(n);
        if (QueryDosDeviceA(name.c_str(), target, sizeof(target)) != 0) found.push_back(name);
    }
    return found;
}

SerialPort::~SerialPort() { close(); }

bool SerialPort::open(const std::string& name, uint32_t baud_rate) {
    close();
    name_ = name;
    if (name_.empty() && !choose_adapter(list_adapters(), &name_, &last_error_)) return false;

    // The \\.\ prefix is required for COM10 and above, and harmless below.
    const std::string path = "\\\\.\\" + name_;
    HANDLE handle = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                OPEN_EXISTING, 0, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        last_error_ = format_last_win_error("cannot open " + name_);
        return false;
    }

    DCB dcb{};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(handle, &dcb)) {
        last_error_ = format_last_win_error("GetCommState failed");
        CloseHandle(handle);
        return false;
    }

    // 8N1, no flow control.
    dcb.BaudRate = baud_rate;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fParity = FALSE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fAbortOnError = FALSE;

    if (!SetCommState(handle, &dcb)) {
        last_error_ = format_last_win_error("SetCommState failed (baud rate not supported?)");
        CloseHandle(handle);
        return false;
    }

    // A servo that is there answers within about a millisecond, so 50 ms of silence means
    // nobody is home. Kept short because a bus scan pays it for every missing ID.
    COMMTIMEOUTS timeouts{};
    timeouts.ReadIntervalTimeout = 20;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutConstant = 100;
    if (!SetCommTimeouts(handle, &timeouts)) {
        last_error_ = format_last_win_error("SetCommTimeouts failed");
        CloseHandle(handle);
        return false;
    }

    handle_ = handle;
    flush();
    last_error_.clear();
    return true;
}

void SerialPort::close() {
    if (handle_) {
        CloseHandle(static_cast<HANDLE>(handle_));
        handle_ = nullptr;
    }
}

bool SerialPort::write(const uint8_t* data, size_t length) {
    DWORD written = 0;
    if (!WriteFile(static_cast<HANDLE>(handle_), data, static_cast<DWORD>(length), &written,
                   nullptr)) {
        last_error_ = format_last_win_error("write failed");
        return false;
    }
    if (written != length) {
        last_error_ = "short write";
        return false;
    }
    return true;
}

size_t SerialPort::read(uint8_t* buffer, size_t length) {
    DWORD got = 0;
    if (!ReadFile(static_cast<HANDLE>(handle_), buffer, static_cast<DWORD>(length), &got,
                  nullptr)) {
        last_error_ = format_last_win_error("read failed");
        return 0;
    }
    return static_cast<size_t>(got);
}

void SerialPort::flush() {
    if (handle_) PurgeComm(static_cast<HANDLE>(handle_), PURGE_RXCLEAR | PURGE_TXCLEAR);
}
