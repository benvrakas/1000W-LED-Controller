#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Power Subsystem Configuration
// Includes: PSU control mode, analog control parameters, CAN Bus settings
// ---------------------------------------------------------------------------

namespace PsuControlConfig {
    // Selects which control path PsuService drives.
    //   true  -> UHP-1500-48CAN: PMBus over native CAN (full telemetry, watchdog).
    //   false -> UHP-1500-48 base: blind analog control on the PC pin (CN71 pin 2).
    // The CAN driver code is always compiled in so this can be flipped if a
    // CAN-capable PSU is later swapped in.
    static constexpr bool PSU_CONTROL_VIA_CAN = false;
}

namespace AnalogPsuConfig {
    // Mapping for the UHP-1500-48 PC pin (constant-current programming).
    // Datasheet: PC accepts 0.4 V (=20% rated) ... 4.7 V (=100% rated) linearly.
    // PSU rated current is 31.5 A; LED hardware ceiling is 22.0 A.
    //
    // Driving from a 3.3 V SAMD51 PWM through an external RC low-pass into the
    // PC pin: 3.3 V max input -> ~74% rated -> ~23.3 A (over the LED ceiling),
    // so we clamp the upper PWM duty to land exactly on 22.0 A.
    static constexpr float PC_VOLTAGE_AT_20PCT      = 0.4f;   // V
    static constexpr float PC_VOLTAGE_AT_100PCT     = 4.7f;   // V
    // Deliberate operating floor for "armed but at minimum" -- kept slightly
    // above PC_VOLTAGE_AT_20PCT (the datasheet's bare minimum for the linear
    // region) for margin. Knob-at-minimum resolves to this voltage while
    // armed, never to 0V: the datasheet calls anything below the 20%-rated
    // point non-linear/undefined, and on the real UHP-1500 a 0V PC command
    // appears to fall back to an unregulated ~48V mode instead of 0% output,
    // which is the opposite of "dim." See PsuService::update().
    static constexpr float PC_VOLTAGE_MIN_ARMED     = 0.5f;   // V
    // Kept equal to CanBusConfig::MAX_CURRENT_A below -- both describe the
    // same UHP-1500-48 rated current, just for the two different control paths.
    static constexpr float PSU_RATED_CURRENT_A      = 31.3f;  // UHP-1500-48 spec
    static constexpr float MCU_PWM_RAIL_V           = 3.3f;   // SAMD51 GPIO rail

    // Duty floor: matches PC_VOLTAGE_MIN_ARMED above, the lowest voltage this
    // board ever commands while armed. (255 * 0.5/3.3 ~= 38.6, rounded to 39.)
    static constexpr uint8_t PWM_DUTY_FLOOR_8BIT    = 39;     // ~15% duty -> ~0.5 V

    // Settle time between starting to drive the PC pin and raising Remote,
    // so the external RC filter (1kOhm + 10uF, tau = RC = 10ms -- see
    // Electronics_Architecture.md) has physically reached a valid voltage
    // before the PSU is enabled to read it. Digital PWM duty updates
    // instantly, but the analog PC voltage behind the RC filter doesn't --
    // a cold 0V->target step crosses the 0.4V deterministic-region boundary
    // at ~1.6 tau (~16ms) and is >95% settled by ~3 tau (~30ms). Raising
    // Remote before that lets the PSU sample PC while it's still transiting
    // the undefined sub-0.4V zone, which it appears to read as "invalid" and
    // answer with a brief full-power/unregulated flash. 50ms (5 tau, >99%
    // settled) leaves real margin. See PsuService::update().
    static constexpr unsigned long PC_SETTLE_MS     = 50UL;

    // Duty ceiling: chosen so the resulting PC voltage maps to MAX_LED_CURRENT_A
    // (set in PsuConfig below). Computed at runtime in AnalogPsuBackend.

    // Hardware watchdog feedback (DC-OK on CN71 pin 6) is optional. When not
    // wired, the analog backend trusts the PSU to follow Remote ON/OFF.
    static constexpr bool   DC_OK_FEEDBACK_WIRED    = false;
}

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
    // 110% of the COB's 22.0A nominal rating -- a deliberate overclock.
    // Chosen as a cautious first step: the UHP-1500 drives this in
    // constant-current mode, which rules out the classic Vf-drop thermal-
    // runaway feedback loop, so the real cost here is shorter LED lifetime
    // and somewhat worse lumens-per-watt, not a cliff -- acceptable given
    // this LED only needs to survive on the order of hundreds of hours
    // total, not thousands. No verified SOA/derating data exists for this
    // part (see docs/Hardware/GETIAN_Product Guide.pdf, a marketing
    // catalog, not a real datasheet), so treat this as a starting point to
    // monitor (sys.globalLedTemp) rather than a confirmed-safe ceiling --
    // adjust down if temperature climbs faster than the stock-current
    // baseline would suggest.
    static constexpr float MAX_LED_CURRENT_A = 24.2f; // 22.0A * 1.10

    // --- Slew Rates (Percent per Second) ---
    static constexpr float SLEW_RATE_NORMAL_PCT_PER_SEC   = 30.0f;   // 30%/s
    static constexpr float SLEW_RATE_SHUTDOWN_PCT_PER_SEC = 100.0f;  // 100%/s
}
