#include "drivers/Mcp2515CanBackend.h"

using namespace Mcp2515Registers;

Mcp2515CanBackend::Mcp2515CanBackend(uint8_t csPin, uint8_t clockMHz)
    : _csPin(csPin), _clockMHz(clockMHz), _initialized(false) {}

void Mcp2515CanBackend::spiBegin() {
    digitalWrite(_csPin, LOW);
}

void Mcp2515CanBackend::spiEnd() {
    digitalWrite(_csPin, HIGH);
}

void Mcp2515CanBackend::reset() {
    spiBegin();
    SPI.transfer(RESET);
    spiEnd();
    delay(10);  // Wait for reset to complete
}

uint8_t Mcp2515CanBackend::readRegister(uint8_t addr) {
    spiBegin();
    SPI.transfer(READ);
    SPI.transfer(addr);
    uint8_t value = SPI.transfer(0x00);
    spiEnd();
    return value;
}

void Mcp2515CanBackend::writeRegister(uint8_t addr, uint8_t value) {
    spiBegin();
    SPI.transfer(WRITE);
    SPI.transfer(addr);
    SPI.transfer(value);
    spiEnd();
}

void Mcp2515CanBackend::modifyRegister(uint8_t addr, uint8_t mask, uint8_t value) {
    spiBegin();
    SPI.transfer(BIT_MODIFY);
    SPI.transfer(addr);
    SPI.transfer(mask);
    SPI.transfer(value);
    spiEnd();
}

bool Mcp2515CanBackend::setMode(uint8_t mode) {
    modifyRegister(CANCTRL, MODE_MASK, mode);
    
    // Wait for mode change with timeout
    uint32_t start = millis();
    while ((readRegister(CANSTAT) & MODE_MASK) != mode) {
        if (millis() - start > 100) {
            return false;  // Timeout
        }
    }
    return true;
}

bool Mcp2515CanBackend::setBitrate(uint32_t bitrate) {
    // Enter configuration mode
    if (!setMode(MODE_CONFIG)) {
        return false;
    }

    // Configure bit timing for the requested bitrate
    // These values are for a 16 MHz crystal
    // TQ = 2 * (BRP + 1) / Fosc
    // Bit Time = Sync + Prop + PS1 + PS2 = 1 + (PRSEG+1) + (PHSEG1+1) + (PHSEG2+1)
    
    uint8_t cnf1, cnf2, cnf3;
    
    if (_clockMHz == 16) {
        switch (bitrate) {
            case 500000:
                // 16 TQ, BRP=0, Sync=1, Prop=4, PS1=4, PS2=7
                cnf1 = 0x00;  // SJW=1, BRP=0
                cnf2 = 0x90;  // BTLMODE=1, SAM=0, PHSEG1=2, PRSEG=0
                cnf3 = 0x02;  // PHSEG2=2
                break;
            case 250000:
                // 16 TQ at 250kbps
                cnf1 = 0x01;  // SJW=1, BRP=1
                cnf2 = 0x90;
                cnf3 = 0x02;
                break;
            case 125000:
                // 16 TQ at 125kbps
                cnf1 = 0x03;  // SJW=1, BRP=3
                cnf2 = 0x90;
                cnf3 = 0x02;
                break;
            default:
                return false;  // Unsupported bitrate
        }
    } else if (_clockMHz == 8) {
        switch (bitrate) {
            case 500000:
                cnf1 = 0x00;
                cnf2 = 0x90;
                cnf3 = 0x02;
                break;
            case 250000:
                cnf1 = 0x00;
                cnf2 = 0xB1;
                cnf3 = 0x05;
                break;
            case 125000:
                cnf1 = 0x01;
                cnf2 = 0xB1;
                cnf3 = 0x05;
                break;
            default:
                return false;
        }
    } else {
        return false;  // Unsupported clock frequency
    }

    writeRegister(CNF1, cnf1);
    writeRegister(CNF2, cnf2);
    writeRegister(CNF3, cnf3);

    return true;
}

bool Mcp2515CanBackend::begin(uint32_t bitrate) {
    // Initialize SPI
    pinMode(_csPin, OUTPUT);
    digitalWrite(_csPin, HIGH);
    SPI.begin();
    SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));

    // Reset the MCP2515
    reset();

    // Configure bitrate
    if (!setBitrate(bitrate)) {
        return false;
    }

    // Configure receive buffers to accept all messages (no filtering)
    writeRegister(RXB0CTRL, 0x64);  // Turn mask/filters off, receive any standard/extended
    writeRegister(RXB1CTRL, 0x60);

    // Clear interrupt flags
    writeRegister(CANINTF, 0x00);

    // Enable interrupts for RX (optional, we use polling)
    writeRegister(CANINTE, 0x03);  // RX0IE and RX1IE

    // Enter normal mode
    if (!setMode(MODE_NORMAL)) {
        return false;
    }

    _initialized = true;
    return true;
}

bool Mcp2515CanBackend::send(uint32_t id, const uint8_t* data, uint8_t len) {
    if (!_initialized || len > 8) {
        return false;
    }

    // Check if TX buffer 0 is available
    uint8_t txCtrl = readRegister(TXB0CTRL);
    if (txCtrl & 0x08) {
        return false;  // Buffer not available (TXREQ still set)
    }

    // Determine if this is extended ID (29-bit)
    bool isExtended = (id > 0x7FF);

    spiBegin();
    SPI.transfer(LOAD_TX0);

    if (isExtended) {
        // Extended ID frame
        uint8_t sidh = (uint8_t)(id >> 21);
        uint8_t sidl = (uint8_t)((id >> 13) & 0xE0) | 0x08 | (uint8_t)((id >> 16) & 0x03);
        uint8_t eid8 = (uint8_t)(id >> 8);
        uint8_t eid0 = (uint8_t)id;

        SPI.transfer(sidh);      // TXB0SIDH
        SPI.transfer(sidl);      // TXB0SIDL (EXIDE = 1)
        SPI.transfer(eid8);      // TXB0EID8
        SPI.transfer(eid0);      // TXB0EID0
    } else {
        // Standard ID frame
        SPI.transfer((uint8_t)(id >> 3));  // TXB0SIDH
        SPI.transfer((uint8_t)(id << 5));  // TXB0SIDL (EXIDE = 0)
        SPI.transfer(0x00);                // TXB0EID8
        SPI.transfer(0x00);                // TXB0EID0
    }

    SPI.transfer(len);  // DLC

    for (uint8_t i = 0; i < len; i++) {
        SPI.transfer(data[i]);
    }
    spiEnd();

    // Request transmission
    spiBegin();
    SPI.transfer(RTS_TX0);
    spiEnd();

    return true;
}

bool Mcp2515CanBackend::receive(uint32_t& id, uint8_t* data, uint8_t& len) {
    if (!_initialized) {
        return false;
    }

    // Check RX status
    spiBegin();
    SPI.transfer(RX_STATUS);
    uint8_t status = SPI.transfer(0x00);
    spiEnd();

    uint8_t rxBuffer = 0xFF;
    if (status & 0x40) {
        rxBuffer = 0;  // Message in RXB0
    } else if (status & 0x80) {
        rxBuffer = 1;  // Message in RXB1
    } else {
        return false;  // No message available
    }

    // Read the message
    spiBegin();
    SPI.transfer(rxBuffer == 0 ? READ_RX0 : READ_RX1);

    uint8_t sidh = SPI.transfer(0x00);
    uint8_t sidl = SPI.transfer(0x00);
    uint8_t eid8 = SPI.transfer(0x00);
    uint8_t eid0 = SPI.transfer(0x00);
    uint8_t dlc  = SPI.transfer(0x00);

    len = dlc & 0x0F;
    if (len > 8) len = 8;

    for (uint8_t i = 0; i < len; i++) {
        data[i] = SPI.transfer(0x00);
    }
    spiEnd();

    // Reconstruct ID
    if (sidl & 0x08) {
        // Extended ID
        id = ((uint32_t)sidh << 21) |
             ((uint32_t)(sidl & 0xE0) << 13) |
             ((uint32_t)(sidl & 0x03) << 16) |
             ((uint32_t)eid8 << 8) |
             (uint32_t)eid0;
    } else {
        // Standard ID
        id = ((uint32_t)sidh << 3) | ((uint32_t)sidl >> 5);
    }

    // Clear the interrupt flag
    modifyRegister(CANINTF, rxBuffer == 0 ? 0x01 : 0x02, 0x00);

    return true;
}
