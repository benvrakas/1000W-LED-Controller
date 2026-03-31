#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Power Subsystem Configuration
// Includes: PSU parameters, CAN Bus settings
// ---------------------------------------------------------------------------

namespace CanBusConfig {
    // --- Bus Settings ---
    static constexpr uint32_t BITRATE             = 250000UL;   // 250 kbps (Mean Well Default)

    // --- Mean Well UHP-1500-48 CAN Protocol (PMBUS over CAN) ---
    // Base address is 0x00 for units without DIP switches
    static constexpr uint8_t  PSU_ADDRESS         = 0x00;
    
    // Command IDs
    // Master -> PSU (Write): 0x000C0300 + Address
    // PSU -> Master (Read):  0x000C0380 + Address (for telemetry)
    static constexpr uint32_t ID_PSU_WRITE        = 0x000C0300 + PSU_ADDRESS;
    static constexpr uint32_t ID_PSU_READ         = 0x000C0380 + PSU_ADDRESS;

    // PMBus Command Codes used as Data[0] in CAN frames
    static constexpr uint8_t CMD_OPERATION        = 0x01;
    static constexpr uint8_t CMD_VOUT_COMMAND     = 0x21;
    static constexpr uint8_t CMD_IOUT_SET         = 0x46; // IOUT_SET or IOUT_OC_FAULT_LIMIT
    static constexpr uint8_t CMD_READ_VOUT        = 0x8B;
    static constexpr uint8_t CMD_READ_IOUT        = 0x8C;
    static constexpr uint8_t CMD_READ_TEMP        = 0x8D;
    static constexpr uint8_t CMD_STATUS_WORD      = 0x79;

    // --- UHP-1500-48 Specifications ---
    static constexpr float MAX_VOLTAGE_V          = 55.2f;      // Max output voltage
    static constexpr float NOMINAL_VOLTAGE_V      = 48.0f;      // Nominal voltage
    static constexpr float MAX_CURRENT_A          = 31.3f;      // Max output current at 48V
    static constexpr float MAX_POWER_W            = 1500.0f;    // Max power

    // --- Watchdog ---
    static constexpr uint32_t TELEMETRY_TIMEOUT_MS = 1000UL;    // Relaxed to 1s for better stability
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
