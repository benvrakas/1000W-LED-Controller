#include "drivers/CanBus.h"
#include "config/PinMap.h"
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Mean Well UHP-1500-48 uses PMBus Linear11 encoding for voltage/current.
// Linear11: 16-bit value = (exponent << 11) | mantissa
//   - exponent: signed 5-bit (bits 15–11)
//   - mantissa: signed 11-bit (bits 10–0)
//   - value = mantissa * 2^exponent
// ---------------------------------------------------------------------------

static float decodeLinear11(uint16_t raw) {
    // Extract exponent (signed 5-bit)
    int16_t exp = static_cast<int16_t>(raw >> 11);
    if (exp & 0x10) {  // sign extend if negative
        exp |= 0xFFE0;
    }
    // Extract mantissa (signed 11-bit)
    int16_t mant = static_cast<int16_t>(raw & 0x07FF);
    if (mant & 0x0400) {  // sign extend if negative
        mant |= 0xF800;
    }
    // Compute value
    if (exp >= 0) {
        return static_cast<float>(mant) * static_cast<float>(1 << exp);
    } else {
        return static_cast<float>(mant) / static_cast<float>(1 << (-exp));
    }
}

static uint16_t encodeLinear11(float value, int exponent) {
    // Encode a float to Linear11 with given exponent
    // mantissa = value / 2^exponent
    float scaledMant;
    if (exponent >= 0) {
        scaledMant = value / static_cast<float>(1 << exponent);
    } else {
        scaledMant = value * static_cast<float>(1 << (-exponent));
    }
    int16_t mant = static_cast<int16_t>(scaledMant + 0.5f);
    // Clamp mantissa to 11-bit signed range
    if (mant > 1023)  mant = 1023;
    if (mant < -1024) mant = -1024;
    // Pack: exponent in bits 15–11, mantissa in bits 10–0
    uint16_t expBits = static_cast<uint16_t>(exponent & 0x1F) << 11;
    uint16_t mantBits = static_cast<uint16_t>(mant & 0x07FF);
    return expBits | mantBits;
}

CanBusManager::CanBusManager(uint8_t txPin, uint8_t rxPin)
    : _txPin(txPin), _rxPin(rxPin), _backend(nullptr),
      _targetPowerPct(0.0f), _currentPowerPct(0.0f), _lastSetpointMs(0),
      _lastTelemetryReqMs(0),
      _telemetry{0.0f, 0.0f, 0.0f, 0, false}, _fault(false)
{}

void CanBusManager::begin(ICanBackend* backend) {
    _backend = backend;

    // Ensure PSU enable is defined and starts in a safe (off) state
    pinMode(PinMap::PIN_PSU_REMOTE, OUTPUT);
    digitalWrite(PinMap::PIN_PSU_REMOTE, LOW);

    if (_backend) {
        _backend->begin(CanBusConfig::BITRATE);
    }

    _lastSetpointMs     = millis();
    _telemetry.lastUpdateMs = millis();
    _fault              = false;
}

void CanBusManager::update(uint32_t nowMs) {
    if (_backend == nullptr) {
        return; // no backend attached yet
    }

    // Periodically request telemetry from PSU
    if ((nowMs - _lastTelemetryReqMs) >= CanBusConfig::TELEMETRY_REQUEST_INTERVAL_MS) {
        requestTelemetry();
        _lastTelemetryReqMs = nowMs;
    }

    processIncoming(nowMs);
    handleWatchdog(nowMs);
    applySlewAndTransmit(nowMs);
}

void CanBusManager::requestTelemetry() {
    if (_backend == nullptr) return;

    // Mean Well UHP-1500-48: Send telemetry request (empty frame triggers response)
    _backend->send(CanBusConfig::ID_TELEMETRY_REQ, nullptr, 0);
}

void CanBusManager::setOperation(bool enable) {
    if (_backend == nullptr) return;

    // Simple 1-byte payload: 0 = off, 1 = on
    uint8_t data[1];
    data[0] = enable ? 1u : 0u;
    _backend->send(CanBusConfig::ID_OPERATION, data, 1);

    // Hardware kill line should never be high when we command off
    if (!enable) {
        digitalWrite(PinMap::PIN_PSU_REMOTE, LOW);
    }
}

void CanBusManager::requestPowerPercent(float pct) {
    if (pct < 0.0f)  pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    _targetPowerPct = pct;
}

bool CanBusManager::hasFault() const {
    return _fault;
}

bool CanBusManager::telemetryValid() const {
    return _telemetry.valid;
}

CanTelemetry CanBusManager::getTelemetry() const {
    return _telemetry;
}

void CanBusManager::processIncoming(uint32_t nowMs) {
    if (_backend == nullptr) return;

    uint32_t id;
    uint8_t  data[8];
    uint8_t  len;

    // Drain the rx queue; keep only the latest telemetry snapshot
    while (_backend->receive(id, data, len)) {
        if (id == CanBusConfig::ID_TELEMETRY_RESP && len >= 6) {
            // Mean Well UHP-1500-48 telemetry response format:
            //   Bytes 0-1: Output voltage (Linear11, little-endian)
            //   Bytes 2-3: Output current (Linear11, little-endian)
            //   Bytes 4-5: Temperature (Linear11, little-endian)
            uint16_t rawVoltage = static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
            uint16_t rawCurrent = static_cast<uint16_t>(data[2]) | (static_cast<uint16_t>(data[3]) << 8);
            uint16_t rawTemp    = static_cast<uint16_t>(data[4]) | (static_cast<uint16_t>(data[5]) << 8);

            _telemetry.voltage     = decodeLinear11(rawVoltage);
            _telemetry.current     = decodeLinear11(rawCurrent);
            _telemetry.temperature = decodeLinear11(rawTemp);
            _telemetry.lastUpdateMs = nowMs;
            _telemetry.valid       = true;
            _fault                 = false; // clear comms fault on good frame
        }
    }
}

void CanBusManager::handleWatchdog(uint32_t nowMs) {
    uint32_t elapsed = nowMs - _telemetry.lastUpdateMs;
    if (elapsed > CanBusConfig::TELEMETRY_TIMEOUT_MS) {
        // Telemetry timeout: drop hardware enable and latch a fault
        digitalWrite(PinMap::PIN_PSU_REMOTE, LOW);
        _fault = true;
    }
}

void CanBusManager::applySlewAndTransmit(uint32_t nowMs) {
    if (_backend == nullptr) return;

    // 30% per second maximum slew, expressed as %/ms
    float maxDeltaPerMs = CanBusConfig::MAX_DELTA_PERCENT_PER_SEC / 1000.0f;
    uint32_t dt = nowMs - _lastSetpointMs;
    if (dt == 0) return;

    float maxStep = maxDeltaPerMs * static_cast<float>(dt);
    float error   = _targetPowerPct - _currentPowerPct;

    if (error > maxStep) {
        error = maxStep;
    } else if (error < -maxStep) {
        error = -maxStep;
    }

    if (error == 0.0f) {
        return; // within allowed band, nothing to do
    }

    _currentPowerPct += error;
    _lastSetpointMs = nowMs;

    // Convert 0–100% to actual current in amps for UHP-1500-48
    // Max current at 48V = 31.3A
    float targetCurrentA = (_currentPowerPct / 100.0f) * CanBusConfig::MAX_CURRENT_A;

    // Encode current setpoint using Linear11 format (PMBus standard)
    // Use exponent = 0 for simplicity (mantissa = integer amps * 256 / 32 ~= x8)
    // For better precision: exponent = -3 gives 0.125A resolution
    uint16_t linear11Current = encodeLinear11(targetCurrentA, -3);

    uint8_t payload[2];
    payload[0] = static_cast<uint8_t>(linear11Current & 0xFF);         // Low byte
    payload[1] = static_cast<uint8_t>((linear11Current >> 8) & 0xFF);  // High byte
    _backend->send(CanBusConfig::ID_SET_CURRENT, payload, 2);
}

