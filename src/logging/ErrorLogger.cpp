#include "logging/ErrorLogger.h"
#include "core/Hardware.h"
#include <Adafruit_SPIFlash.h>
#include <SdFat.h>

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

    // Initialize filesystem
    // flash.begin() is already called in initHardware()
    if (!fatfs.begin(&flash)) {
        // Failed to mount filesystem. 
        // We could try to format here:
        // if (flash.eraseChip()) { fatfs.format(&flash); }
        // But for safety, we'll just operate in RAM-only mode for now.
    }

    _initialized = true;
}

void ErrorLogger::appendRecord(const LogRecord &record) {
    // 1. Update in-memory ring buffer
    _records[_head] = record;
    _head = (_head + 1U) % MAX_RECORDS;

    if (_count < MAX_RECORDS) {
        ++_count;
    }

    // 2. Persist to QSPI Flash
    File logFile = fatfs.open("error_log.csv", FILE_WRITE);
    if (logFile) {
        // If file is empty, write header
        if (logFile.size() == 0) {
            logFile.println("Time,Fault,State,BootStep,LED_C,Water_C,MainRPM,AuxRPM,PsuRPM,PumpRPM,Volts,Amps,Watts");
        }

        logFile.print(record.timestampMs);
        logFile.print(",");
        logFile.print(static_cast<int>(record.fault));
        logFile.print(",");
        logFile.print(static_cast<int>(record.state));
        logFile.print(",");
        logFile.print(record.bootStep);
        logFile.print(",");
        
        logFile.print(record.ledTempC);
        logFile.print(",");
        logFile.print(record.pumpTempC);
        logFile.print(",");
        
        logFile.print(record.mainFansRPM);
        logFile.print(",");
        logFile.print(record.auxFanRPM);
        logFile.print(",");
        logFile.print(record.psuFanRPM);
        logFile.print(",");
        logFile.print(record.pumpRPM);
        logFile.print(",");

        logFile.print(record.psuVoltage);
        logFile.print(",");
        logFile.print(record.psuCurrent);
        logFile.print(",");
        logFile.println(record.psuPower);

        logFile.close();
    }
}

void ErrorLogger::update(const SystemController& sys, const SystemViewModel& vm, unsigned long now) {
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
    rec.state       = sys.currentState;
    rec.bootStep    = sys.initData.bootStep;

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
}

