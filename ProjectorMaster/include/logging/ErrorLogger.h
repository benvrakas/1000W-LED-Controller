#pragma once

#include "state/SystemController.h"
#include "core/SystemViewModel.h"
#include "logging/FaultManager.h"

// ErrorLogger
// -----------
// Responsible for persisting fault events and key telemetry to external
// QSPI flash (via a lower-level LogStore abstraction to be implemented).
//
// In this first iteration, ErrorLogger maintains an in-RAM ring buffer of
// recent fault records. When a concrete LogStore implementation is
// available, these records can be flushed to QSPI for long-term storage.

class ErrorLogger {
 public:
    struct LogRecord {
        unsigned long timestampMs;   // millis() at time of log
        FaultCode      fault;        // Fault that triggered this record
        SystemState    state;        // High-level system state
        uint8_t        bootStep;     // Boot step if in INIT (0 otherwise)

        // Snapshot of key telemetry at the time of the fault
        float    ledTempC;
        float    pumpTempC;
        uint16_t mainFansRPM;
        uint16_t auxFanRPM;
        uint16_t psuFanRPM;
        uint16_t pumpRPM;

        float psuVoltage;
        float psuCurrent;
        float psuPower;
    };

    static ErrorLogger &instance();

    // Initialize logging back-end (QSPI, file system layout, etc.). Called
    // once from RUN when logging is first needed.
    void begin();

    // Periodic update: detect new faults from FaultManager and append
    // structured log records to persistent storage (or in-RAM buffer for
    // this iteration). Receives system state and a view model snapshot.
    void update(const SystemController& sys, const SystemViewModel& vm, unsigned long now);

 private:
    ErrorLogger();

    static constexpr size_t MAX_RECORDS = 32;

    void appendRecord(const LogRecord &record);

    LogRecord _records[MAX_RECORDS];
    size_t    _head;        // Next write index in the ring buffer
    size_t    _count;       // Number of valid records in the buffer
    bool      _initialized; // True after begin() has been called

    FaultCode _lastLoggedFault;
};

