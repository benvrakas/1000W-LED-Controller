#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Power Subsystem Configuration
// Includes: PSU parameters, CAN Bus settings
// ---------------------------------------------------------------------------

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

    // --- Watchdog ---
    static constexpr uint32_t TELEMETRY_TIMEOUT_MS = 500UL;     // 500 ms
    static constexpr uint32_t TELEMETRY_REQUEST_INTERVAL_MS = 100UL;  // Request every 100ms

    // --- Slew-Rate Limiter ---
    static constexpr float MAX_DELTA_PERCENT_PER_SEC = 30.0f;
}

namespace PsuConfig {
    // --- LED Load Limits ---
    static constexpr float MAX_LED_CURRENT_A = 22.0f; // COB nominal current limit

    // --- Slew Rates (Percent per Second) ---
    static constexpr float SLEW_RATE_NORMAL_PCT_PER_SEC   = 30.0f;   // 30%/s
    static constexpr float SLEW_RATE_SHUTDOWN_PCT_PER_SEC = 100.0f;  // 100%/s
}
