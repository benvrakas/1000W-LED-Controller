#include "logging/ErrorLogger.h"
#include "core/Hardware.h"
#include <Adafruit_SPIFlash.h>
#include <SdFat.h>
#include <Adafruit_SleepyDog.h>

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
    if (_initialized) return;

    Serial.println(F("FS: Mounting QSPI Flash..."));
    Watchdog.reset();
    
    if (!fatfs.begin(&flash)) {
        Serial.println(F("FS: Mount Failed (unformatted?)"));
    } else {
        Serial.println(F("FS: Mount OK"));
    }

    _initialized = true;
}

void ErrorLogger::appendRecord(const LogRecord &record) {
    _records[_head] = record;
    _head = (_head + 1U) % MAX_RECORDS;
    if (_count < MAX_RECORDS) ++_count;

    if (!_initialized) return;

    // TEMPORARILY DISABLED: Checking for bootloop cause
    /*
    Serial.println(F("FS: Writing Log..."));
    ...
    */
    Serial.println(F("FS: Flash write skipped (debug mode)"));
}

void ErrorLogger::update(const SystemController& sys, const SystemViewModel& vm, unsigned long now) {
    FaultManager &fm = FaultManager::instance();
    if (!fm.hasActiveFaults()) return;

    FaultCode currentFault = fm.getActiveFault();
    if (currentFault == _lastLoggedFault || currentFault == FaultCode::NONE) return;

    if (!_initialized) begin();

    LogRecord rec = {};
    rec.timestampMs = now;
    rec.fault       = currentFault;
    rec.state       = sys.currentState;
    rec.bootStep    = sys.initData.bootStep;
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
