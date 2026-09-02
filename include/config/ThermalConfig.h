#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Thermal Subsystem Configuration
// Includes: Thermistors, Tachometers (Fans/Pump), and Cooling Curves
// ---------------------------------------------------------------------------

namespace ThermistorConfig {
    static constexpr float MAX_TEMP_LED = 75.0f;
    static constexpr float MAX_TEMP_PUMP = 50.0f;

    static constexpr float BETA_VALUE_LED = 3950.0f;
    static constexpr float BETA_VALUE_PUMP = 3950.0f;

    static constexpr float NOMINAL_RESISTANCE_LED = 10000.0f; // 10K NTC
    static constexpr float NOMINAL_RESISTANCE_PUMP = 10000.0f; // 10K NTC

    static constexpr uint32_t SERIES_RESISTOR_LED = 10000UL; // 10K Fixed
    static constexpr uint32_t SERIES_RESISTOR_PUMP = 10000UL; // 10K Fixed

    // Hardware Orientation:
    // true  = 3.3V -- [Thermistor] -- PIN -- [Resistor] -- GND
    // false = 3.3V -- [Resistor] -- PIN -- [Thermistor] -- GND
    static constexpr bool IS_HIGH_SIDE_LED = false; 
    static constexpr bool IS_HIGH_SIDE_PUMP = false;
}

namespace TachometerConfig {
    // Deadstart Duty Cycles (minimum PWM to start spinning)
    static constexpr uint8_t  MAIN_PSU_DEADSTART_DUTY = 77;
    // NMB 12038VA-24R datasheet: "Please use the start with Duty 30% or
    // more at 25kHz" for reliable startup. NOTE: the aux/lens fan actually
    // installed is a 12038VA-24Q-EM (confirmed 4-wire/PWM+tach, but a lower
    // speed grade than the -24R part this figure -- and the fixed 25000Hz
    // passed to auxFan's constructor in Hardware.cpp -- are sourced from).
    // No Q-class-specific PWM datasheet has been found, so this 30%/25kHz
    // figure is carried over unconfirmed for that part. See
    // docs/System Overview/Hardware.md.
    static constexpr uint8_t  AUX_DEADSTART_DUTY = 77; // ~30%
    static constexpr uint8_t  PUMP_DEADSTART_DUTY = 127;

    // Spin-up duty applied during INIT. The DEADSTART values above are the
    // minimum duty that keeps an already-turning rotor turning -- they are
    // NOT enough to reliably break a stopped rotor loose (the NMB 12038VA
    // datasheet calls 30% the bare minimum for starting). INIT drives every
    // channel at 50% duty for SPINUP_MS, verifies the tach, and only then
    // hands off to CoolingService's PI-controlled duty.
    static constexpr uint8_t  MAIN_PSU_SPINUP_DUTY = 128;
    static constexpr uint8_t  AUX_SPINUP_DUTY      = 128;
    static constexpr uint8_t  PUMP_SPINUP_DUTY     = 128;
    static constexpr unsigned long SPINUP_MS = 1000UL;

    // Per-boot-step ceiling in handleInitState() before INIT_FAILED latches.
    // A healthy pump/fan clears SPINUP_MS and crosses its stall RPM
    // threshold well within this window; if it hasn't by then it isn't
    // going to (dead unit, disconnected tach, etc.), so there's no benefit
    // to waiting longer -- it only delays surfacing a real failure.
    static constexpr unsigned long BOOT_STEP_TIMEOUT_MS = 2000UL;

    // RPM Computation Intervals (milliseconds)
    static constexpr unsigned long RPM_COMPUTE_INTERVAL = 200;

    // Window that stall detection is evaluated over. getRPM() keeps the
    // RPM_COMPUTE_INTERVAL cadence above so the UI stays responsive, but a
    // 200ms window at 2 pulses/rev quantizes to 30000/200 = 150 RPM per
    // pulse -- a fan actually turning at 200 RPM reads 0 or 150, straddling
    // the ~300 RPM stall thresholds below. Summing pulses over this longer
    // window puts ~10 pulses behind the stall decision instead of ~2.
    // Must be a whole multiple of RPM_COMPUTE_INTERVAL.
    static constexpr unsigned long STALL_EVAL_WINDOW_MS = 1000UL;

    // RPM Limits (for scaling/clamping)
    static constexpr uint16_t MAX_MAIN_PSU_RPM = 3000;
    static constexpr uint16_t MAX_AUX_RPM = 6000;
    static constexpr uint16_t MAX_PUMP_RPM = 4800;

    // Stall Detection Thresholds (RPM below this with duty > 0 = stall)
    static constexpr uint16_t MAIN_PSU_STALL_RPM = 300;
    static constexpr uint16_t AUX_STALL_RPM = 300;
    static constexpr uint16_t PUMP_STALL_RPM = 150;

    // Temporary: the PSU fan's tach reading is not currently trusted (still
    // being debugged), and it's the least safety-critical channel, so its
    // stall check is disabled everywhere -- INIT verification (fansVerify),
    // runtime fault detection (FaultManager::update), and INIT_FAILED/
    // COOLING_FAILURE channel attribution all skip it. Its PWM is still
    // driven normally by the fan curve; only the tach-based fault gating is
    // off. Flip back to true once the PSU fan's tach signal is trusted again.
    static constexpr bool PSU_FAN_TACH_MONITORING_ENABLED = false;

    // Spin-up grace period: stall detection is suppressed for this long
    // after duty goes from 0 to nonzero, since a healthy fan/pump takes time
    // to accelerate from a stop and won't be above the stall RPM immediately.
    static constexpr unsigned long STALL_GRACE_MS = 1000UL;

    // Once the grace period above has elapsed, a stall reading must persist
    // continuously for this long before COOLING_FAILURE latches -- RPM is
    // only recomputed every RPM_COMPUTE_INTERVAL, so a single sample right at
    // the grace-period boundary can catch a fan still mid-ramp-up and
    // falsely read as stalled for one instant. Mirrors the debounce already
    // used for the overtemp checks in FaultManager.
    static constexpr unsigned long STALL_FAULT_DEBOUNCE_MS = 500UL;
}

namespace FanCurveConfig {
    // Limits
    static constexpr uint8_t MAIN_DUTY_MIN = 64;  // ~25%
    static constexpr uint8_t MAIN_DUTY_MAX = 255; // 100%

    static constexpr uint8_t PUMP_DUTY_MIN = 77;  // ~30%
    static constexpr uint8_t PUMP_DUTY_MAX = 255; // 100%

    static constexpr uint8_t AUX_DUTY_MIN = 51;   // ~20%
    static constexpr uint8_t AUX_DUTY_MAX = 255;  // 100%

    // PSU fan and aux/lens fan: fixed linear relationship to the LED's
    // actual applied duty (PsuService::getAppliedCurrentFraction()) any
    // time the LED is on, instead of the water-temp PI curve main fan uses
    // -- a deliberate, simple proportional rule rather than a
    // thermally-reactive one. See CoolingService::update().
    static constexpr float PSU_FAN_TO_LED_DUTY_RATIO = 0.20f; // 20%
    static constexpr float AUX_FAN_TO_LED_DUTY_RATIO  = 0.50f; // 50%

    // PI Controller Configuration (Target Temp & Tuning). Drives the main
    // radiator fan (plus the aux/PSU fans' LED-off fallback, which mirror
    // it) -- see CoolingService::update(). The pump has no PI loop; it's
    // either PUMP_DUTY_MAX (LED on) or PUMP_DUTY_MIN (LED off), fixed.
    static constexpr float TARGET_TEMP_WATER = 40.0f;
    static constexpr float WATER_KP = 2.00f;
    static constexpr float WATER_KI = 0.060f;
    static constexpr float WATER_INTEGRAL_MAX = 100.0f;
}
