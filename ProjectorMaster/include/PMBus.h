#pragma once

#include <Wire.h>

namespace PMBusConfig {
    // --- Connection Settings ---
    static constexpr uint8_t DEFAULT_ADDRESS = 0x40;
    static constexpr uint32_t BUS_SPEED      = 100000; // 100kHz standard

    // --- Opperating Parameter Constraints ---

    // --- Linear 11 Exponents (Specific to UHP-1500) ---
    // These tell the math engine how to shift the bits
    static constexpr int8_t EXP_VOLTAGE     = -9; //Check documentation to confirm
    static constexpr int8_t EXP_CURRENT     = -4;
    static constexpr int8_t EXP_TEMPERATURE = 0;
}

namespace PMBusCommands {
    static constexpr uint8_t VOUT_COMMAND    = 0x21; // Set Voltage
    static constexpr uint8_t OPERATION       = 0x01; // Power On/Off
    static constexpr uint8_t STATUS_WORD     = 0x79; // Fault status
    static constexpr uint8_t READ_VOUT       = 0x8B; // Read Actual Volts
    static constexpr uint8_t READ_IOUT       = 0x8C; // Read Actual Amps
    static constexpr uint8_t READ_TEMP_1     = 0x8D; // Internal Temp
    static constexpr uint8_t MFR_ID          = 0x99; // Manufacturer Name
}

class PMBusManager {
public:
    // Constructor: Takes the I2C address (Default 0x40)
    PMBusManager(uint8_t sdaPin, uint8_t sclPin, uint8_t address);

    // Initialization (Call this in setup)
    void begin(TwoWire* i2cBus = &Wire);

    // Getters
    bool  hasFault();
    
    // Setters
    void sendCommand(uint8_t hexCommand);

private:
    TwoWire*  _bus;
    uint8_t  _address;

    // Helper: Linear11 conversion
    float    decodeLinear11(uint16_t raw);
    uint16_t encodeLinear11(float value, int8_t exponent);

    // Low-level I2C read/write
    uint16_t readWord(uint8_t cmd);
    void     writeWord(uint8_t cmd, uint16_t data);
};

// Extern declaration so 'psu' is available globally
extern PMBusManager psu;