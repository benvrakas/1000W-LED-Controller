#pragma once

#include "core/SystemViewModel.h"

// UiController
// ------------
// Mediates between the system state/telemetry and the OLED driver. This
// controller owns what is drawn on screen: PSU stats, circular LED power
// gauge, virtual encoder position, and error banners/screens.

class UiController {
public:
    UiController();

    // Initialize fonts, layout state, and any cached references to the
    // underlying OledManager. Called once when entering RUN.
    void begin();

    // Periodic update from RUN. Responsible for selecting the active UI
    // screen and issuing drawing commands based on the current system
    // state (normal vs error) and telemetry.
    void update(const SystemViewModel& vm, unsigned long now);
};

