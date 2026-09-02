#pragma once

#include "state/SystemController.h"

class TachometerManager;

// FaultCode
// ---------
// Enumerates high-level fault conditions the system can experience. This
// list can be extended as new fault conditions are implemented.

enum class FaultCode : uint8_t {
    NONE = 0,

    // PSU / power-path related faults
    CAN_TIMEOUT,        // Lost CAN communication with PSU
    PSU_FAULT,          // PSU reported an internal fault status

    // Thermal and cooling faults
    OVER_TEMP_LED,      // LED junction / MCPCB exceeded safe temperature
    OVER_TEMP_WATER,    // Coolant temperature exceeded safe limit
    COOLING_FAILURE,    // Fan or pump RPM too low for commanded duty

    // UI / input faults
    ENCODER_FAULT,
    // System faults
    INIT_FAILED,        // Boot sequence timeout or failure
};

// IgnorableSource
// ---------------
// The eight monitored channels a fault can be attributed to, matching the
// diagnostics screen's pages 1-8 one-to-one. NONE covers anything that
// doesn't map to one of them (a board-pins INIT sanity failure, or a
// corrupted boot step) -- these have no channel to add to an ignore list,
// so the ERROR_KILL 3s hold has no effect on them; only a physical power
// cycle clears them.
enum class IgnorableSource : uint8_t {
    NONE = 0,
    LED_TEMP, WATER_TEMP, PUMP, MAIN_FAN, PSU_FAN, AUX_FAN, PSU_COMMS, ENCODER
};

// Maps a fault (and its detail string, for the codes that need one to
// disambiguate) to the channel responsible. Shared by the ERROR_KILL 3s
// hold (which channel to add to the per-session ignore list) and the
// diagnostics screen (which page is "<<CAUSED SHUTDOWN"), so both agree.
IgnorableSource identifyFaultSource(FaultCode active, const char* detail);

// FaultSnapshot
// -------------
// A capture of the values that mattered at the moment a fault was raised.
// FaultManager only ever latches one FaultCode as "the" active fault, but
// the diagnostics screen (ERROR_KILL) wants to show every channel's
// last-recorded value, not just the one that tripped -- this is what makes
// that possible. Free-standing (not nested in FaultManager) so headers that
// only need the type can forward-declare it without pulling in
// state/SystemController.h transitively via FaultManager.h.
struct FaultSnapshot {
    float    ledTempC   = 0.0f;
    float    waterTempC = 0.0f;
    uint16_t pumpRPM = 0, mainFanRPM = 0, psuFanRPM = 0, auxFanRPM = 0;
    uint8_t  pumpDuty = 0, mainFanDuty = 0, psuFanDuty = 0, auxFanDuty = 0;
    bool     canFaultFlag = false;       // CanBusManager::hasFault() at trip time
    bool     canTelemetryValid = false;  // CanBusManager::telemetryValid() at trip time
    uint16_t encoderIllegalAccum = 0;    // FaultManager's windowed illegal-transition count
    unsigned long timestampMs = 0;
};

// FaultManager
// ------------
// Central registry for active faults. Provides a single place where
// subsystems raise or clear faults, and where the RUN and ERROR_KILL
// states query overall system health.

class FaultManager {
 public:
    static FaultManager &instance();

    void raiseFault(FaultCode code, const char* detail = nullptr);
    void clearFault(FaultCode code);

    // Drops every in-flight debounce timer (overtemp, per-channel stall, the
    // encoder window) without touching the latched fault itself. Called from
    // SystemController::transitionTo(): these timers are only meaningful
    // within one continuous stretch of RUN, and a stale one means the first
    // bad sample after a state change satisfies its debounce immediately and
    // latches with no debounce at all.
    void resetDebounceTimers();

    bool        hasActiveFaults() const;
    FaultCode   getActiveFault() const;   // Expose current fault for logging/telemetry
    const char* getFaultDetail() const;   // Optional sub-detail (e.g. which cooling channel)
    const FaultSnapshot& getSnapshot() const; // System values as of the last raiseFault()

    // Optional periodic work; can be used to implement time-based fault
    // transitions or latching behavior.
    void update(SystemController &sys, unsigned long now);

    // Illegal-transition burst threshold used by ENCODER_FAULT detection;
    // exposed so the diagnostics screen can show "count / threshold".
    static constexpr uint16_t ENCODER_FAULT_THRESHOLD  = 50;   // Illegal transitions within window to fault

 private:
    FaultManager();

    // Debounced stall check shared by pump/mainFan/psuFan/auxFan -- returns
    // true only once TachometerManager::getStallStatus() has read true
    // continuously for TachometerConfig::STALL_FAULT_DEBOUNCE_MS.
    bool checkStallDebounced(TachometerManager& fan, unsigned long& stallStartMs, unsigned long now);

    FaultCode   _activeFault;
    const char* _faultDetail;
    FaultSnapshot _snapshot;

    // Temporal filtering for noisy sensors
    unsigned long _ledOvertempStartMs;
    unsigned long _waterOvertempStartMs;
    static constexpr unsigned long THERMAL_FAULT_DEBOUNCE_MS = 1000; // Must persist for 1s

    // Temporal filtering for the cooling-stall checks (see checkStallDebounced)
    unsigned long _pumpStallStartMs;
    unsigned long _mainFanStallStartMs;
    unsigned long _psuFanStallStartMs;
    unsigned long _auxFanStallStartMs;

    // Windowed burst detection for illegal encoder quadrature transitions.
    // Threshold is deliberately loose: with the EIC glitch filter disabled
    // (see SystemStartup::isrInit), ordinary mechanical contact bounce at
    // each detent -- plus the encoder ISR's low NVIC priority relative to
    // the button/tach ISRs -- both produce occasional double-bit reads
    // during completely normal knob use. A genuinely failing/disconnected
    // channel produces illegal transitions continuously, at a rate far
    // higher than this. Confirmed on hardware that 3/1s trips during normal
    // use; 50/1s is chosen for real separation margin. Still a bench-tunable
    // value, not a hard spec -- watch FaultManager.cpp's debug log if it
    // needs revisiting.
    unsigned long _encoderFaultWindowStartMs;
    uint16_t      _encoderIllegalAccum;
    static constexpr unsigned long ENCODER_FAULT_WINDOW_MS  = 1000; // Burst window
};

