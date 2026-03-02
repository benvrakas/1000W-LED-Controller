#pragma once

#include <Arduino.h>

// Lightweight, backend-agnostic CAN manager for the Mean Well UHP-1500-48

namespace CanBusConfig {
    // --- Bus Settings ---
    static constexpr uint32_t BITRATE             = 500000UL;   // 500 kbps

    // --- Mean Well UHP-1500-48 CAN Protocol (PMBUS over CAN) ---
    // Base address is configurable via DIP switches; default = 0x00
    // CAN IDs use 29-bit extended format: 0x000C03XX where XX = unit address
    static constexpr uint8_t  PSU_ADDRESS         = 0x40;       // Default PSU address
    
    // Command IDs (add to base 0x000C0300 + address)
    static constexpr uint32_t ID_OPERATION        = 0x000C0340; // On/Off command
    static constexpr uint32_t ID_SET_CURRENT      = 0x000C0341; // Set current (IOUT_OC_WARN_LIMIT)
    static constexpr uint32_t ID_SET_VOLTAGE      = 0x000C0342; // Set voltage (VOUT_COMMAND)
    static constexpr uint32_t ID_TELEMETRY_REQ    = 0x000C0343; // Request telemetry
    static constexpr uint32_t ID_TELEMETRY_RESP   = 0x000C0380; // Telemetry response (broadcast)

    // --- UHP-1500-48 Specifications ---
    static constexpr float MAX_VOLTAGE_V          = 55.2f;      // Max output voltage
    static constexpr float NOMINAL_VOLTAGE_V      = 48.0f;      // Nominal voltage
    static constexpr float MAX_CURRENT_A          = 31.3f;      // Max output current at 48V
    static constexpr float MAX_POWER_W            = 1500.0f;    // Max power

    // --- Linear11 Encoding (PMBus format) ---
    // Mean Well uses Linear11 format for voltage/current values
    // Value = Mantissa * 2^Exponent where Mantissa is 11-bit signed, Exp is 5-bit signed

    // --- Watchdog ---
    static constexpr uint32_t TELEMETRY_TIMEOUT_MS = 500UL;     // 500 ms
    static constexpr uint32_t TELEMETRY_REQUEST_INTERVAL_MS = 100UL;  // Request every 100ms

    // --- Slew-Rate Limiter ---
    static constexpr float MAX_DELTA_PERCENT_PER_SEC = 30.0f;
}

// Abstract backend interface so any CAN driver can be used
class ICanBackend {
public:
    virtual ~ICanBackend() = default;

    virtual bool begin(uint32_t bitrate) = 0;
    virtual bool send(uint32_t id, const uint8_t* data, uint8_t len) = 0;
    virtual bool receive(uint32_t& id, uint8_t* data, uint8_t& len) = 0;
};

struct CanTelemetry {
    float    voltage;        // Volts
    float    current;        // Amps
    float    temperature;    // Deg C (internal)
    uint32_t lastUpdateMs;   // millis() of last valid telemetry frame
    bool     valid;          // true if we have at least one good frame
};

class CanBusManager {
public:
    // txPin / rxPin are the MCU pins wired to the CAN transceiver (ISO1050)
    CanBusManager(uint8_t txPin, uint8_t rxPin);

    // Attach a concrete backend and bring up the bus
    void begin(ICanBackend* backend);

    // Call as frequently as possible from the main loop/state machine
    void update(uint32_t nowMs);

    // High-level controls
    void setOperation(bool enable);        // PSU on/off (0x000C0340)
    void requestPowerPercent(float pct);   // 0–100% with 30%/s internal ramping

    // Status / telemetry
    bool         hasFault() const;         // true if watchdog tripped or other error
    bool         telemetryValid() const;   // at least one telemetry frame rx'd
    CanTelemetry getTelemetry() const;     // last cached telemetry snapshot

private:
    uint8_t     _txPin;
    uint8_t     _rxPin;
    ICanBackend* _backend;

    // Slew-rate limiting state
    float    _targetPowerPct;   // requested by higher-level logic (0–100)
    float    _currentPowerPct;  // last command actually sent to PSU
    uint32_t _lastSetpointMs;   // millis() when we last adjusted _currentPowerPct

    // Telemetry + watchdog
    uint32_t     _lastTelemetryReqMs;   // millis() when we last sent telemetry request
    CanTelemetry _telemetry;
    bool         _fault;            // sticky fault flag when watchdog trips

    void requestTelemetry();                // send 0x000C0343 to request status
    void processIncoming(uint32_t nowMs);   // handle rx frames & telemetry
    void handleWatchdog(uint32_t nowMs);   // enforce 500 ms telemetry timeout
    void applySlewAndTransmit(uint32_t nowMs); // enforce 30%/s limit & send 0x000C0341
};

// Extern declaration so 'psu' is available globally (defined in main.cpp)
extern CanBusManager psu;

