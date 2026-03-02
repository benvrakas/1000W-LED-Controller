#include "ui/UiController.h"

UiController::UiController() = default;

void UiController::begin() {
    // TODO: Acquire access to the global OledManager instance and perform
    // any one-time initialization for the UI (splash screen, initial
    // layout, etc.).
}

void UiController::update(SystemController &sys, unsigned long now) {
    (void)sys;
    (void)now;
    // TODO: Render PSU stats, LED power gauge, virtual encoder position,
    // and either normal or error screens depending on FaultManager state.
}

