#include "drivers/Tachometers.h"

// pinPeripheral() - needed so the fixed-frequency PWM path can re-assert its
// own pin mux; see writeDutyToTimer().
#include "wiring_private.h"

// ---------------------------------------------------------------------------
// TachometerManager - PWM output + tachometer RPM feedback (fan curve driven)
// ---------------------------------------------------------------------------

TachometerManager::TachometerManager(uint8_t pwmPin, uint8_t tachPin,
                                     uint8_t minDeadStart, uint16_t maxRPM, uint16_t stallRPM,
                                     uint32_t fixedPwmFreqHz)
    : _pwmPin(pwmPin),
      _tachPin(tachPin),
      _minDeadStart(minDeadStart),
      _maxRPM(maxRPM),
      _stallRPM(stallRPM),
      _fixedPwmFreqHz(fixedPwmFreqHz),
      _fixedFreqTc(nullptr),
      _fixedFreqChannel(0),
      _fixedFreqPeriod(0),
      _currentDuty(0),
      _pulseCount(0),
      _currentRPM(0),
      _peakRPM(0),
      _lastRPMCompute(0),
      _dutyOnSinceMs(0),
      _histPulses{},
      _histDurationMs{},
      _histIndex(0),
      _histCount(0)
{}

void TachometerManager::begin() {
    pinMode(_pwmPin, OUTPUT);

    // Deliberately NOT pinMode(_tachPin, INPUT) here. SystemStartup::
    // boardPinsInit() already sets it INPUT in boot step 1, before isrInit()
    // attaches this pin's interrupt to the EIC -- calling pinMode() again
    // here, after that attachment, clears PINCFG.PMUXEN and silently
    // disconnects the pin from the EIC, exactly like the aux fan's PWM pin
    // (see writeDutyToTimer()'s comment) but on the tach side instead.
    // begin() runs from pumpInit()/fansInit() in step 2/3, always after step
    // 1, so the pin is already correctly configured by the time we get here.
    //
    // Tach signals are push-pull CMOS from ADuM isolators, already
    // referenced to the clean 3.3V domain. No pull-up required.

    analogWriteResolution(8);
    analogWrite(_pwmPin, 0);

    // Start the RPM window here rather than leaving _lastRPMCompute at 0.
    // Otherwise the first calculateRPM() call spans everything since
    // power-on -- pulses counted over milliseconds of INIT divided by
    // seconds of elapsed time, which reads as a near-zero RPM regardless of
    // what the rotor is actually doing.
    _lastRPMCompute = millis();
    resetStallHistory();

    // analogWrite() above does the one-time pin mux / GCLK / timer-enable
    // setup at the core's default prescaler+period; only override it after
    // that has run, since a later analogWrite() call would otherwise reset
    // our prescaler/period back to default on this timer's next first-use.
    if (_fixedPwmFreqHz > 0) {
        configureFixedFrequencyPwm();
    }
}

void TachometerManager::configureFixedFrequencyPwm() {
    PinDescription pinDesc = g_APinDescription[_pwmPin];
    uint32_t tcNum = GetTCNumber(pinDesc.ulPWMChannel);
    uint8_t tcChannel = GetTCChannelNumber(pinDesc.ulPWMChannel);

    // Only plain TC timers are handled here - this core always runs them in
    // 8-bit COUNT8 mode (PER max 255), unlike TCC which has a wider counter.
    // No fan currently needs a fixed frequency on a TCC-mapped pin.
    if (tcNum < TCC_INST_NUM) {
        Serial.print(F("Tach: pin "));
        Serial.print(_pwmPin);
        Serial.println(F(" is TCC-mapped, not TC -- fixed-frequency PWM NOT configured, falling back to default rate"));
        return;
    }

    static const uint16_t kDivisors[] = {1, 2, 4, 8, 16, 64, 256, 1024};
    static const uint32_t kPrescalerRegs[] = {
        TC_CTRLA_PRESCALER_DIV1,   TC_CTRLA_PRESCALER_DIV2,
        TC_CTRLA_PRESCALER_DIV4,   TC_CTRLA_PRESCALER_DIV8,
        TC_CTRLA_PRESCALER_DIV16,  TC_CTRLA_PRESCALER_DIV64,
        TC_CTRLA_PRESCALER_DIV256, TC_CTRLA_PRESCALER_DIV1024,
    };

    uint32_t bestPeriod = 0;
    uint32_t bestPrescalerReg = 0;
    uint32_t bestErrorHz = 0xFFFFFFFFu;

    for (uint8_t i = 0; i < 8; i++) {
        uint32_t divisor = kDivisors[i];
        uint32_t denom = divisor * _fixedPwmFreqHz;
        uint32_t rawPeriod = (F_CPU + denom / 2) / denom; // round to nearest
        if (rawPeriod < 1 || rawPeriod > 256) continue;
        uint32_t achievedHz = F_CPU / (divisor * rawPeriod);
        uint32_t errorHz = (achievedHz > _fixedPwmFreqHz)
            ? (achievedHz - _fixedPwmFreqHz)
            : (_fixedPwmFreqHz - achievedHz);
        if (errorHz < bestErrorHz) {
            bestErrorHz = errorHz;
            bestPeriod = rawPeriod;
            bestPrescalerReg = kPrescalerRegs[i];
        }
    }

    if (bestPeriod == 0) {
        Serial.print(F("Tach: pin "));
        Serial.print(_pwmPin);
        Serial.print(F(" has no valid TC divider for "));
        Serial.print(_fixedPwmFreqHz);
        Serial.println(F("Hz -- fixed-frequency PWM NOT configured, falling back to default rate"));
        return; // no valid divider for this frequency; keep default
    }

    Tc* TCx = (Tc*) GetTC(pinDesc.ulPWMChannel);

    TCx->COUNT8.CTRLA.bit.ENABLE = 0;
    while (TCx->COUNT8.SYNCBUSY.bit.ENABLE);

    TCx->COUNT8.CTRLA.reg = TC_CTRLA_MODE_COUNT8 | bestPrescalerReg;
    TCx->COUNT8.WAVE.reg = TC_WAVE_WAVEGEN_NPWM;

    while (TCx->COUNT8.SYNCBUSY.bit.PER);
    TCx->COUNT8.PER.reg = (uint8_t)(bestPeriod - 1);

    while (TCx->COUNT8.SYNCBUSY.bit.CC0);
    TCx->COUNT8.CC[tcChannel].reg = 0;

    TCx->COUNT8.CTRLA.bit.ENABLE = 1;
    while (TCx->COUNT8.SYNCBUSY.bit.ENABLE);

    _fixedFreqTc = TCx;
    _fixedFreqChannel = tcChannel;
    _fixedFreqPeriod = (uint8_t)(bestPeriod - 1);
}

void TachometerManager::setDuty(uint8_t duty) {
    // Apply dead-start minimum if duty is non-zero but below threshold
    if (duty > 0 && duty < _minDeadStart) {
        duty = _minDeadStart;
    }
    // Detect the 0->nonzero transition right here, since this is the only
    // place _currentDuty actually changes. Timestamping this in update()
    // instead would lag a full cycle behind the transition (update() runs
    // before setDuty() in the caller's loop), leaving a window where
    // getStallStatus() could fire before the grace period ever started.
    //
    // Read millis() directly rather than a timestamp cached by update():
    // setDuty() is also called from INIT and from the ERROR_KILL overheat
    // failsafe, neither of which calls update() first. A cached-zero
    // timestamp lands as _dutyOnSinceMs == 0, which getStallStatus() reads
    // as "no grace period at all".
    if (duty > 0 && _currentDuty == 0) {
        _dutyOnSinceMs = millis();
        // Samples taken while the rotor was stopped say nothing about
        // whether it's stalled now.
        resetStallHistory();
        // Same reasoning: a peak from a previous run shouldn't make this
        // run look healthier than it actually is.
        _peakRPM = 0;
    } else if (duty == 0) {
        _dutyOnSinceMs = 0;
    }
    _currentDuty = duty;
    writeDutyToTimer(_currentDuty);
}

void TachometerManager::writeDutyToTimer(uint8_t duty) {
    if (_fixedFreqTc != nullptr) {
        // Re-assert the pin mux before every write. The analogWrite() path
        // below gets this for free (analogWrite() calls pinPeripheral() on
        // every call), but writing CC directly does not - and any
        // pinMode(_pwmPin, OUTPUT) elsewhere clears PINCFG.PMUXEN wholesale,
        // silently disconnecting the pin from the timer while this driver
        // happily keeps updating a register nothing is listening to.
        // SystemStartup::boardPinsInit() does exactly that on every INIT
        // pass, which is what left the aux fan free-running with no PWM at
        // all after a fault clear.
        pinPeripheral(_pwmPin, PIO_TIMER);

        // analogWrite() writes the raw 0-255 value straight into CC, which
        // would overrun our shorter fixed-frequency period - scale to it
        // and write the timer directly instead.
        uint8_t cc = (uint8_t)(((uint32_t)duty * _fixedFreqPeriod) / 255);
        while (_fixedFreqTc->COUNT8.SYNCBUSY.bit.CC0);
        _fixedFreqTc->COUNT8.CC[_fixedFreqChannel].reg = cc;
    } else {
        analogWrite(_pwmPin, duty);
    }
}

uint16_t TachometerManager::getRPM() const {
    return _currentRPM;
}

uint16_t TachometerManager::getPeakRPM() const {
    return _peakRPM;
}

uint8_t TachometerManager::getDuty() const {
    return _currentDuty;
}

uint32_t TachometerManager::getPulseCount() const {
    return _pulseCount;
}

void TachometerManager::resetStallHistory() {
    _histIndex = 0;
    _histCount = 0;
}

uint16_t TachometerManager::getWindowedRPM() const {
    if (_histCount == 0) return _currentRPM;

    uint32_t pulses = 0;
    uint32_t durationMs = 0;
    for (uint8_t i = 0; i < _histCount; i++) {
        pulses     += _histPulses[i];
        durationMs += _histDurationMs[i];
    }
    if (durationMs == 0) return _currentRPM;

    uint32_t rpm = (pulses * 30000UL) / durationMs;
    if (rpm > _maxRPM) rpm = _maxRPM;
    return static_cast<uint16_t>(rpm);
}

bool TachometerManager::getStallStatus() const {
    if (_currentDuty == 0) return false;
    if (_dutyOnSinceMs != 0 && (millis() - _dutyOnSinceMs) < TachometerConfig::STALL_GRACE_MS) {
        return false; // still within spin-up grace period, too soon to judge
    }
    // Judged on the longer window, not the latest 200ms sample - see
    // TachometerConfig::STALL_EVAL_WINDOW_MS.
    return getWindowedRPM() <= _stallRPM;
}

void TachometerManager::stop() {
    // Route through setDuty() rather than poking _currentDuty directly, so
    // _dutyOnSinceMs is cleared with it and a later restart is guaranteed to
    // re-arm the spin-up grace period.
    setDuty(0);
}

void TachometerManager::stopSlow() {
    // Gradually reduce duty cycle - call repeatedly from main loop
    if (_currentDuty > 0) {
        uint8_t next = (_currentDuty > 10) ? (_currentDuty - 10) : 0;
        // Step below _minDeadStart goes straight to 0: setDuty() would
        // otherwise clamp it back up to the dead-start floor and the ramp
        // would never reach zero.
        if (next < _minDeadStart) next = 0;
        setDuty(next);
    }
}

void TachometerManager::handleTachoInterrupt() {
    _pulseCount++;
}

void TachometerManager::update(unsigned long currentMillis) {
    calculateRPM(currentMillis);
}

void TachometerManager::calculateRPM(unsigned long currentMillis) {
    unsigned long duration = currentMillis - _lastRPMCompute;

    if (duration >= TachometerConfig::RPM_COMPUTE_INTERVAL) {
        // Atomically read and clear pulse count
        noInterrupts();
        uint32_t capturedPulses = _pulseCount;
        _pulseCount = 0;
        interrupts();

        // RPM = (pulses per period) * (60000 ms/min) / (period in ms) / 2 (pulses per rev)
        // Simplified: RPM = capturedPulses * 30000 / duration
        uint32_t rpm = (capturedPulses * 30000UL) / duration;

        // Clamp to max RPM
        if (rpm > _maxRPM) {
            rpm = _maxRPM;
        }
        _currentRPM = static_cast<uint16_t>(rpm);
        if (_currentRPM > _peakRPM) {
            _peakRPM = _currentRPM;
        }

        // Feed the rolling window that stall detection is judged on. Raw
        // pulses and duration, not the clamped RPM above, so the window sums
        // exactly.
        _histPulses[_histIndex]     = capturedPulses;
        _histDurationMs[_histIndex] = duration;
        _histIndex = (_histIndex + 1) % STALL_HISTORY_SLOTS;
        if (_histCount < STALL_HISTORY_SLOTS) _histCount++;

        _lastRPMCompute = currentMillis;
    }
}

// ISR Bridge Functions are defined in src/util/HardwareBridges.cpp
