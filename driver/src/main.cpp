// main.cpp -- command line tool for setting up STS3215 servos.
//
//   servo_tool COM5 scan                list every servo answering on the bus
//   servo_tool COM5 ping 1              check whether one servo answers
//   servo_tool COM5 setid 1 3           rename servo 1 to servo 3
//   servo_tool COM5 pos 3               read a servo's current position
//   servo_tool COM5 assign 4            walk through giving 4 servos IDs 1..4
//
// Add --baud <rate> before the command if a servo is not on the factory
// 1000000 baud.
#include "serial_port.h"
#include "sts3215.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void print_usage() {
    std::cout <<
        "usage: servo_tool <PORT> [--baud N] <command> [args]\n"
        "\n"
        "commands:\n"
        "  scan                 ping every ID from 0 to 253 and list the ones that answer\n"
        "  ping <id>            check a single servo\n"
        "  setid <old> <new>    change a servo's ID (one servo on the bus, please)\n"
        "  pos <id>             read the servo's present position (0-4095)\n"
        "  read <id> <reg> [n]  read register <reg>, n = 1 or 2 bytes (default 1)\n"
        "  write <id> <reg> <v> [n] [--force]   write <v> to <reg>, n = 1 or 2 bytes\n"
        "  assign [count]       interactive: give `count` servos the IDs 1..count\n"
        "\n"
        "registers are decimal or 0x hex. Useful ones:\n"
        "  40 torque enable (0/1)   55 EEPROM lock (0/1)   62 voltage (0.1V)\n"
        "  63 temperature (C)       42 goal position (2B)  56 present position (2B)\n"
        "\n"
        "writes to registers below 40 are EEPROM (persistent, write-limited) and\n"
        "need --force. Writing register 6 (baud rate) can make a servo unreachable.\n"
        "\n"
        "example: servo_tool COM5 assign 4\n";
}

// Parses a servo ID and complains if it is outside 0-253.
bool parse_id(const char* text, uint8_t* out) {
    char* end = nullptr;
    const long value = std::strtol(text, &end, 10);
    if (end == text || *end != '\0' || value < 0 || value > 253) {
        std::cout << "'" << text << "' is not a valid servo ID (0-253)\n";
        return false;
    }
    *out = static_cast<uint8_t>(value);
    return true;
}

// Parses a register address or register value. Accepts decimal or 0x hex.
bool parse_u8(const char* text, const char* what, uint8_t* out) {
    char* end = nullptr;
    const long value = std::strtol(text, &end, 0);
    if (end == text || *end != '\0' || value < 0 || value > 255) {
        std::cout << "'" << text << "' is not a valid " << what << " (0-255)\n";
        return false;
    }
    *out = static_cast<uint8_t>(value);
    return true;
}

// Registers below this address live in EEPROM: they survive power-off, are
// write-limited, and include the ID and baud rate. Writing the baud rate by
// accident makes the servo unreachable, so those writes need --force.
constexpr uint8_t kFirstSramRegister = 40;

void wait_for_enter(const std::string& prompt) {
    std::cout << prompt << std::flush;
    std::string ignored;
    std::getline(std::cin, ignored);
}

// Walks the user through one servo at a time. Only one servo may be plugged in
// at a time: a brand new servo is ID 1, so two of them on the same bus would
// both answer to the same name and the renaming would hit whichever shouts
// loudest.
int run_assign(sts3215::Bus& bus, int count) {
    std::cout << "\nAssigning IDs 1 to " << count << ".\n"
              << "Plug in exactly ONE servo at a time -- unplug the others.\n";

    for (int target = 1; target <= count; ++target) {
        std::cout << "\n--- servo " << target << " of " << count << " ---\n";
        wait_for_enter("Connect the servo that should become ID " +
                       std::to_string(target) + ", then press Enter: ");

        std::cout << "scanning the bus...\n";
        const std::vector<uint8_t> found = bus.scan();

        if (found.empty()) {
            std::cout << "No servo answered. Check power, the data cable, and the baud rate,\n"
                         "then run the tool again.\n";
            return 1;
        }
        if (found.size() > 1) {
            std::cout << "Found " << found.size()
                      << " servos on the bus. Leave only one connected and try again.\n";
            return 1;
        }

        const uint8_t current = found[0];
        std::cout << "found servo with ID " << static_cast<int>(current) << "\n";

        if (current == target) {
            std::cout << "already ID " << target << ", nothing to do.\n";
            continue;
        }

        if (!bus.set_id(current, static_cast<uint8_t>(target))) {
            std::cout << "failed: " << bus.last_error() << "\n";
            return 1;
        }
        std::cout << "servo is now ID " << target << "\n";
    }

    std::cout << "\nAll done. Plug every servo back in and run `scan` to confirm\n"
                 "you see IDs 1 to " << count << ".\n";
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        print_usage();
        return 1;
    }

    const std::string port_name = argv[1];
    uint32_t baud_rate = sts3215::kDefaultBaudRate;

    int arg = 2;
    if (std::string(argv[arg]) == "--baud") {
        if (arg + 1 >= argc) {
            std::cout << "--baud needs a number\n";
            return 1;
        }
        baud_rate = static_cast<uint32_t>(std::strtoul(argv[arg + 1], nullptr, 10));
        arg += 2;
    }
    if (arg >= argc) {
        print_usage();
        return 1;
    }

    const std::string command = argv[arg++];
    const int remaining = argc - arg;

    SerialPort port;
    if (!port.open(port_name, baud_rate)) {
        std::cout << port.last_error() << "\n";
        return 1;
    }
    std::cout << "opened " << port_name << " at " << baud_rate << " baud\n";

    sts3215::Bus bus(port);

    if (command == "scan") {
        std::cout << "scanning IDs 0-253, this takes a few seconds...\n";
        const std::vector<uint8_t> found = bus.scan();
        if (found.empty()) {
            std::cout << "no servos found\n";
            return 1;
        }
        std::cout << "found " << found.size() << " servo(s):";
        for (uint8_t id : found) {
            std::cout << " " << static_cast<int>(id);
        }
        std::cout << "\n";
        return 0;
    }

    if (command == "ping") {
        uint8_t id = 0;
        if (remaining < 1 || !parse_id(argv[arg], &id)) {
            print_usage();
            return 1;
        }
        if (!bus.ping(id)) {
            std::cout << "servo " << static_cast<int>(id) << " did not answer ("
                      << bus.last_error() << ")\n";
            return 1;
        }
        std::cout << "servo " << static_cast<int>(id) << " is alive\n";
        return 0;
    }

    if (command == "setid") {
        uint8_t old_id = 0;
        uint8_t new_id = 0;
        if (remaining < 2 || !parse_id(argv[arg], &old_id) || !parse_id(argv[arg + 1], &new_id)) {
            print_usage();
            return 1;
        }
        if (!bus.set_id(old_id, new_id)) {
            std::cout << "failed: " << bus.last_error() << "\n";
            return 1;
        }
        std::cout << "servo " << static_cast<int>(old_id) << " is now servo "
                  << static_cast<int>(new_id) << "\n";
        return 0;
    }

    if (command == "pos") {
        uint8_t id = 0;
        if (remaining < 1 || !parse_id(argv[arg], &id)) {
            print_usage();
            return 1;
        }
        uint16_t position = 0;
        if (!bus.read_u16(id, sts3215::kRegPresentPosition, &position)) {
            std::cout << "failed: " << bus.last_error() << "\n";
            return 1;
        }
        // The STS3215 reports position as 0-4095 over its 360 degree range.
        std::cout << "servo " << static_cast<int>(id) << " position: " << position << " ("
                  << (position * 360.0 / 4096.0) << " degrees)\n";
        return 0;
    }

    if (command == "read") {
        uint8_t id = 0;
        uint8_t reg = 0;
        if (remaining < 2 || !parse_id(argv[arg], &id) ||
            !parse_u8(argv[arg + 1], "register", &reg)) {
            print_usage();
            return 1;
        }

        int width = 1;
        if (remaining >= 3) {
            width = std::atoi(argv[arg + 2]);
            if (width != 1 && width != 2) {
                std::cout << "byte count must be 1 or 2\n";
                return 1;
            }
        }

        if (width == 1) {
            uint8_t value = 0;
            if (!bus.read_u8(id, reg, &value)) {
                std::cout << "failed: " << bus.last_error() << "\n";
                return 1;
            }
            std::cout << "servo " << static_cast<int>(id) << " reg " << static_cast<int>(reg)
                      << " = " << static_cast<int>(value) << "\n";
        } else {
            uint16_t value = 0;
            if (!bus.read_u16(id, reg, &value)) {
                std::cout << "failed: " << bus.last_error() << "\n";
                return 1;
            }
            std::cout << "servo " << static_cast<int>(id) << " reg " << static_cast<int>(reg)
                      << " = " << value << "\n";
        }
        return 0;
    }

    if (command == "write") {
        bool force = false;
        std::vector<const char*> args;
        for (int i = arg; i < argc; ++i) {
            if (std::string(argv[i]) == "--force") {
                force = true;
            } else {
                args.push_back(argv[i]);
            }
        }

        uint8_t id = 0;
        uint8_t reg = 0;
        if (args.size() < 3 || !parse_id(args[0], &id) || !parse_u8(args[1], "register", &reg)) {
            print_usage();
            return 1;
        }

        int width = 1;
        if (args.size() >= 4) {
            width = std::atoi(args[3]);
            if (width != 1 && width != 2) {
                std::cout << "byte count must be 1 or 2\n";
                return 1;
            }
        }

        char* end = nullptr;
        const long raw = std::strtol(args[2], &end, 0);
        const long limit = (width == 1) ? 255 : 65535;
        if (end == args[2] || *end != '\0' || raw < 0 || raw > limit) {
            std::cout << "'" << args[2] << "' is not a valid " << width << "-byte value (0-"
                      << limit << ")\n";
            return 1;
        }

        if (reg < kFirstSramRegister && !force) {
            std::cout << "register " << static_cast<int>(reg)
                      << " is EEPROM: the write persists across power cycles, and register 6\n"
                         "(baud rate) or 5 (ID) can make the servo unreachable. Re-run with\n"
                         "--force if that is really what you want, or use `setid` to change an ID.\n";
            return 1;
        }

        const bool ok = (width == 1)
                            ? bus.write_u8(id, reg, static_cast<uint8_t>(raw))
                            : bus.write_u16(id, reg, static_cast<uint16_t>(raw));
        if (!ok) {
            std::cout << "failed: " << bus.last_error() << "\n";
            return 1;
        }
        std::cout << "servo " << static_cast<int>(id) << " reg " << static_cast<int>(reg)
                  << " <- " << raw << "\n";
        return 0;
    }

    if (command == "assign") {
        int count = 4;
        if (remaining >= 1) {
            count = std::atoi(argv[arg]);
        }
        if (count < 1 || count > 253) {
            std::cout << "count must be between 1 and 253\n";
            return 1;
        }
        return run_assign(bus, count);
    }

    std::cout << "unknown command: " << command << "\n\n";
    print_usage();
    return 1;
}
