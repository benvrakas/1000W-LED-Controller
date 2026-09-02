#pragma once

#include "core/SystemViewModel.h"
#include "drivers/OLED.h"  // Includes Adafruit_SH110X

// Forward declarations only -- avoids UiController.h -> FaultManager.h ->
// state/SystemController.h -> UiController.h include cycle. Full
// definitions are pulled in by UiController.cpp where they're needed.
enum class FaultCode : uint8_t;
struct FaultSnapshot;

// CoolingLiveReadout
// -------------------
// Live RPM/duty for the four cooling channels, read fresh each call --
// unlike FaultSnapshot, which is frozen at the instant a fault first
// tripped. Used by the ERROR_KILL diagnostics screen so a channel that's
// still being actively driven (e.g. the overheat failsafe) doesn't display
// stale numbers from before the fault fired.
struct CoolingLiveReadout {
    uint16_t pumpRPM, mainFanRPM, psuFanRPM, auxFanRPM;
    uint8_t  pumpDuty, mainFanDuty, psuFanDuty, auxFanDuty;
    // Highest RPM each channel reached since its duty last went 0->nonzero
    // (TachometerManager::getPeakRPM()) -- shown alongside the live reading
    // so the diagnostics screen still communicates "how well did this
    // channel spin up" even after ERROR_KILL's failsafe has zeroed its
    // current duty/RPM back out.
    uint16_t pumpPeakRPM, mainFanPeakRPM, psuFanPeakRPM, auxFanPeakRPM;
};

// IgnoredChannels
// ---------------
// Mirrors SystemController's eight per-channel ignore flags (see
// SystemController.h) so the diagnostics screen can show "IGNORED" on a
// page instead of its normal OK/OUT OF RANGE verdict, without UiController.h
// needing to include SystemController.h (would create an include cycle --
// see the forward-declare note above).
struct IgnoredChannels {
    bool ledTemp, waterTemp, pump, mainFan, psuFan, auxFan, psuComms, encoder;
};

// UiController
// ------------
// Mediates between the system state/telemetry and the OLED driver. This
// controller owns what is drawn on screen: PSU stats, circular LED power
// gauge, virtual encoder position, and error banners/screens.

class UiController {
public:
    UiController(OledManager& oled);

    // Initialize fonts, layout state, and any cached references to the
    // underlying OledManager. Called once when entering RUN.
    void begin();

    // Periodic update from RUN. Responsible for selecting the active UI
    // screen and issuing drawing commands based on the current system
    // state (normal vs error) and telemetry.
    void update(const SystemViewModel& vm, unsigned long now);

    // Renders the ERROR_KILL diagnostics screen: a paged view over every
    // monitored channel's last-recorded value (snap), highlighting which
    // one(s) are out of spec and which one is the officially latched cause
    // (active/detail). pageIndex selects the page; navigation itself is
    // owned by the caller (StateErrorKill.cpp), matching how RUN builds a
    // SystemViewModel and hands it to update() above.
    void renderFaultDiagnostics(FaultCode active, const char* detail,
                                 const FaultSnapshot& snap,
                                 const CoolingLiveReadout& live,
                                 const IgnoredChannels& ignored,
                                 uint8_t pageIndex, unsigned long now);

    // Total number of diagnostics pages renderFaultDiagnostics() understands.
    static constexpr uint8_t DIAGNOSTICS_PAGE_COUNT = 9;

    // Expose display for direct access if needed (check isReady first)
    Adafruit_SH1107* getDisplay() { return _oled.getDisplay(); }

private:
    OledManager& _oled;
};

