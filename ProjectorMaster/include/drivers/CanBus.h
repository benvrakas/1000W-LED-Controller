#pragma once

#include <Arduino.h>

// Lightweight, backend-agnostic CAN manager for the Mean Well UHP-1500-48

namespace CanBusConfig {
    // --- Bus Settings ---
    static constexpr uint32_t BITRATE             = 500000UL;   // 500 kbps

    // --- Mean Well UHP-1500-48 CAN IDs ---
    static constexpr uint32_t ID_OPERATION        = 0x000C0340; // On/Off, mode, etc.
    static constexpr uint32_t ID_SET_CURRENT      = 0x000C0341; // Set current / power setpoint
    static constexpr uint32_t ID_TELEMETRY        = 0x000C0342; // Voltage, current, temp, status

    // --- Watchdog ---
    static constexpr uint32_t TELEMETRY_TIMEOUT_MS = 500UL;     // 500 ms

    // --- Slew-Rate Limiter ---
    // Maximum change of commanded power per second (in percent of full scale)
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
    CanTelemetry _telemetry;
    bool         _fault;            // sticky fault flag when watchdog trips

    void processIncoming(uint32_t nowMs);  // handle rx frames & telemetry
    void handleWatchdog(uint32_t nowMs);   // enforce 500 ms telemetry timeout
    void applySlewAndTransmit(uint32_t nowMs); // enforce 30%/s limit & send 0x000C0341
};

// Extern declaration so 'psu' is available globally (defined in main.cpp)
extern CanBusManager psu;

