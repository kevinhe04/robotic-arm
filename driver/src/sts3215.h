// sts3215.h -- the Feetech STS3215 serial bus protocol, by hand.
//
// Every servo on the bus listens to the same two wires. A message is addressed
// to one servo by its ID, so before anything else works each servo needs its
// own ID. That is what this file is for.
//
// Packet we send:
//     0xFF 0xFF  ID  LEN  INSTRUCTION  PARAM...  CHECKSUM
// Packet the servo sends back:
//     0xFF 0xFF  ID  LEN  ERROR        PARAM...  CHECKSUM
//
// LEN counts the bytes after it, i.e. number of params + 2.
// CHECKSUM is the low byte of ~(ID + LEN + INSTRUCTION + all params).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

class SerialPort;

namespace sts3215 {

// Instruction bytes.
constexpr uint8_t kInstPing = 0x01;
constexpr uint8_t kInstRead = 0x02;
constexpr uint8_t kInstWrite = 0x03;

// A packet sent to this ID is obeyed by every servo on the bus, and nobody
// replies. Only ever safe when exactly one servo is connected.
constexpr uint8_t kBroadcastId = 0xFE;

// Addresses in the servo's control table. The ones below 0x28 live in EEPROM
// and survive a power cycle; the rest are RAM and reset on every power-up.
constexpr uint8_t kRegId = 5;               // EEPROM: this servo's ID, 0-253
constexpr uint8_t kRegBaudRate = 6;         // EEPROM: baud index, 0 = 1 Mbaud
constexpr uint8_t kRegLock = 55;            // EEPROM write lock: 0 unlocked, 1 locked
constexpr uint8_t kRegTorqueEnable = 40;    // RAM
constexpr uint8_t kRegGoalPosition = 42;    // RAM, 2 bytes
constexpr uint8_t kRegPresentPosition = 56; // RAM, 2 bytes

constexpr uint32_t kDefaultBaudRate = 1000000;  // factory setting of a new STS3215

// One servo bus. Holds a reference to an already-open serial port.
class Bus {
public:
    explicit Bus(SerialPort& port) : port_(port) {}

    // Is a servo with this ID answering?
    bool ping(uint8_t id);

    // Single-byte register access. Return false if the servo did not reply.
    bool write_u8(uint8_t id, uint8_t reg, uint8_t value);
    bool read_u8(uint8_t id, uint8_t reg, uint8_t* out);

    // Two-byte register access. The STS series stores these low byte first.
    bool read_u16(uint8_t id, uint8_t reg, uint16_t* out);
    bool write_u16(uint8_t id, uint8_t reg, uint16_t value);

    // Pings every ID from 0 to 253 and returns the ones that answered.
    std::vector<uint8_t> scan();

    // Unlock EEPROM -> write the new ID -> lock EEPROM again (addressing the
    // lock write to the NEW id, because the servo has already renamed itself).
    bool set_id(uint8_t current_id, uint8_t new_id);

    const std::string& last_error() const { return last_error_; }

private:
    // Sends one packet and waits for the reply. `params` may be empty.
    // On success `reply_params` holds whatever the servo sent back.
    bool transact(uint8_t id, uint8_t instruction, const std::vector<uint8_t>& params,
                  std::vector<uint8_t>* reply_params, size_t expected_param_count);

    SerialPort& port_;
    std::string last_error_;
};

}  // namespace sts3215
