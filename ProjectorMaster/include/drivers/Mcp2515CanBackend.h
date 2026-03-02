#pragma once

#include "drivers/CanBus.h"
#include <SPI.h>

// MCP2515 SPI CAN Controller Backend
// -----------------------------------
// Concrete ICanBackend implementation for the MCP2515 SPI CAN controller.
// This is a common choice for adding CAN to Arduino boards.
//
// Wiring:
//   MCP2515 VCC -> 3.3V
//   MCP2515 GND -> GND
//   MCP2515 CS  -> User-selected CS pin
//   MCP2515 SCK -> SCK
//   MCP2515 MOSI -> MOSI
//   MCP2515 MISO -> MISO
//   MCP2515 INT -> Optional interrupt pin (not used in polling mode)

namespace Mcp2515Registers {
    // MCP2515 SPI Instructions
    static constexpr uint8_t RESET       = 0xC0;
    static constexpr uint8_t READ        = 0x03;
    static constexpr uint8_t WRITE       = 0x02;
    static constexpr uint8_t READ_STATUS = 0xA0;
    static constexpr uint8_t RX_STATUS   = 0xB0;
    static constexpr uint8_t BIT_MODIFY  = 0x05;
    static constexpr uint8_t LOAD_TX0    = 0x40;
    static constexpr uint8_t LOAD_TX1    = 0x42;
    static constexpr uint8_t LOAD_TX2    = 0x44;
    static constexpr uint8_t RTS_TX0     = 0x81;
    static constexpr uint8_t RTS_TX1     = 0x82;
    static constexpr uint8_t RTS_TX2     = 0x84;
    static constexpr uint8_t READ_RX0    = 0x90;
    static constexpr uint8_t READ_RX1    = 0x94;

    // MCP2515 Register Addresses
    static constexpr uint8_t CANSTAT     = 0x0E;
    static constexpr uint8_t CANCTRL     = 0x0F;
    static constexpr uint8_t CNF3        = 0x28;
    static constexpr uint8_t CNF2        = 0x29;
    static constexpr uint8_t CNF1        = 0x2A;
    static constexpr uint8_t CANINTE     = 0x2B;
    static constexpr uint8_t CANINTF     = 0x2C;
    static constexpr uint8_t TXB0CTRL    = 0x30;
    static constexpr uint8_t TXB0SIDH    = 0x31;
    static constexpr uint8_t RXB0CTRL    = 0x60;
    static constexpr uint8_t RXB1CTRL    = 0x70;

    // CANCTRL modes
    static constexpr uint8_t MODE_NORMAL     = 0x00;
    static constexpr uint8_t MODE_SLEEP      = 0x20;
    static constexpr uint8_t MODE_LOOPBACK   = 0x40;
    static constexpr uint8_t MODE_LISTENONLY = 0x60;
    static constexpr uint8_t MODE_CONFIG     = 0x80;
    static constexpr uint8_t MODE_MASK       = 0xE0;
}

class Mcp2515CanBackend : public ICanBackend {
public:
    // csPin: SPI chip select pin for the MCP2515
    // clockMHz: Crystal oscillator frequency on MCP2515 (typically 8 or 16)
    explicit Mcp2515CanBackend(uint8_t csPin, uint8_t clockMHz = 16);

    // ICanBackend interface
    bool begin(uint32_t bitrate) override;
    bool send(uint32_t id, const uint8_t* data, uint8_t len) override;
    bool receive(uint32_t& id, uint8_t* data, uint8_t& len) override;

private:
    uint8_t _csPin;
    uint8_t _clockMHz;
    bool    _initialized;

    // SPI helpers
    void     spiBegin();
    void     spiEnd();
    void     reset();
    uint8_t  readRegister(uint8_t addr);
    void     writeRegister(uint8_t addr, uint8_t value);
    void     modifyRegister(uint8_t addr, uint8_t mask, uint8_t value);
    bool     setMode(uint8_t mode);
    bool     setBitrate(uint32_t bitrate);
};
