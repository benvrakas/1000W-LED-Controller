#pragma once

#include "systemstate/SystemController.h"
#include "logging/FaultManager.h"

// ErrorLogger
// -----------
// Responsible for persisting fault events and key telemetry to external
// QSPI flash (via a lower-level LogStore abstraction to be implemented).

class ErrorLogger {
public:
    static ErrorLogger &instance();

    // Initialize logging back-end (QSPI, file system layout, etc.). Called
    // once from RUN when logging is first needed.
    void begin();

    // Periodic update: detect new faults from FaultManager and append
    // structured log records to persistent storage.
    void update(SystemController &sys, unsigned long now);

private:
    ErrorLogger();

    FaultCode _lastLoggedFault;
};

