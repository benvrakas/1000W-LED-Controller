#include "logging/ErrorLogger.h"

ErrorLogger &ErrorLogger::instance() {
    static ErrorLogger logger;
    return logger;
}

ErrorLogger::ErrorLogger()
    : _head(0),
      _count(0),
      _initialized(false),
      _lastLoggedFault(FaultCode::NONE) {}

void ErrorLogger::begin() {
    if (_initialized) {
        return;
    }

    _head           = 0;
    _count          = 0;
    _lastLoggedFault = FaultCode::NONE;

    for (size_t i = 0; i < MAX_RECORDS; ++i) {
        _records[i] = {};
    }

    _initialized = true;

    // TODO: Initialize the QSPI-backed LogStore abstraction here so that
    // future fault events can be written to persistent storage.
}

void ErrorLogger::appendRecord(const LogRecord &record) {
    _records[_head] = record;
    _head = (_head + 1U) % MAX_RECORDS;

    if (_count < MAX_RECORDS) {
        ++_count;
    }
}

void ErrorLogger::update(SystemState state, const SystemViewModel& vm, unsigned long now) {
    FaultManager &fm = FaultManager::instance();

    if (!fm.hasActiveFaults()) {
        return;
    }

    FaultCode currentFault = fm.getActiveFault();

    // Only log when the active fault changes, to avoid spamming identical
    // records every cycle.
    if (currentFault == _lastLoggedFault || currentFault == FaultCode::NONE) {
        return;
    }

    LogRecord rec = {};
    rec.timestampMs = now;
    rec.fault       = currentFault;
    rec.state       = state;

    // Snapshot telemetry from the view model
    rec.ledTempC    = vm.ledTempC;
    rec.pumpTempC   = vm.waterTempC;
    rec.mainFansRPM = vm.mainFanRPM;
    rec.auxFanRPM   = vm.auxFanRPM;
    rec.psuFanRPM   = vm.psuFanRPM;
    rec.pumpRPM     = vm.pumpRPM;

    rec.psuVoltage  = vm.psuVoltage;
    rec.psuCurrent  = vm.psuCurrent;
    rec.psuPower    = vm.psuPower;

    appendRecord(rec);
    _lastLoggedFault = currentFault;

    // TODO: When a concrete LogStore exists, enqueue this record for
    // persistent write to QSPI here.
}

