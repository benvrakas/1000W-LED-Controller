#include "logging/ErrorLogger.h"

ErrorLogger &ErrorLogger::instance() {
    static ErrorLogger logger;
    return logger;
}

ErrorLogger::ErrorLogger()
    : _lastLoggedFault(FaultCode::NONE) {}

void ErrorLogger::begin() {
    // TODO: Initialize the QSPI-backed LogStore abstraction here so that
    // future fault events can be written to persistent storage.
}

void ErrorLogger::update(SystemController &sys, unsigned long now) {
    (void)sys;
    (void)now;

    FaultManager &fm = FaultManager::instance();
    if (!fm.hasActiveFaults()) {
        return;
    }

    // TODO: When concrete FaultCodes are defined, compare against
    // _lastLoggedFault and append a new record to persistent storage when
    // a new fault is observed.
}

