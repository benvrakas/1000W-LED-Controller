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
    int16_t exp = static_cast<int16_t>(raw >> 11);
    if (exp & 0x10) exp |= 0xFFE0;
    int16_t mant = static_cast<int16_t>(raw & 0x07FF);
    if (mant & 0x0400) mant |= 0xF800;
    
    if (exp >= 0) return static_cast<float>(mant) * static_cast<float>(1 << exp);
    else return static_cast<float>(mant) / static_cast<float>(1 << (-exp));
}

static uint16_t encodeLinear11(float value, int exponent) {
    float scaledMant;
    if (exponent >= 0) scaledMant = value / static_cast<float>(1 << exponent);
    else scaledMant = value * static_cast<float>(1 << (-exponent));
    int16_t mant = static_cast<int16_t>(scaledMant + 0.5f);
    if (mant > 1023)  mant = 1023;
    if (mant < -1024) mant = -1024;
    uint16_t expBits = static_cast<uint16_t>(exponent & 0x1F) << 11;
    uint16_t mantBits = static_cast<uint16_t>(mant & 0x07FF);
    return expBits | mantBits;
}

CanBusManager::CanBusManager()
    : _backend(nullptr),
      _targetPowerPct(0.0f), _currentPowerPct(0.0f), _lastSetpointMs(0),
      _lastTelemetryReqMs(0),
      _telemetry{0.0f, 0.0f, 0.0f, 0, false}, _fault(false)
{}

void CanBusManager::begin(ICanBackend* backend) {
    _backend = backend;
    pinMode(PinMap::PIN_PSU_REMOTE, OUTPUT);
    digitalWrite(PinMap::PIN_PSU_REMOTE, LOW);
    if (_backend) _backend->begin(CanBusConfig::BITRATE);
    _lastSetpointMs = millis();
    _telemetry.lastUpdateMs = millis();
    _fault = false;
}

void CanBusManager::update(uint32_t nowMs) {
    if (_backend == nullptr) return;
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
    // Mean Well units respond to the READ command on the Write ID
    uint8_t data[1] = {CanBusConfig::CMD_READ_VOUT}; 
    _backend->send(CanBusConfig::ID_PSU_WRITE, data, 1);
}

void CanBusManager::setOperation(bool enable) {
    if (_backend == nullptr) return;
    uint8_t data[2];
    data[0] = CanBusConfig::CMD_OPERATION;
    data[1] = enable ? 0x80u : 0x00u;
    _backend->send(CanBusConfig::ID_PSU_WRITE, data, 2);
    if (!enable) digitalWrite(PinMap::PIN_PSU_REMOTE, LOW);
}

void CanBusManager::requestPowerPercent(float pct) {
    if (pct < 0.0f)  pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    _targetPowerPct = pct;
}

bool CanBusManager::hasFault() const { return _fault; }
bool CanBusManager::telemetryValid() const { return _telemetry.valid; }
CanTelemetry CanBusManager::getTelemetry() const { return _telemetry; }

void CanBusManager::processIncoming(uint32_t nowMs) {
    if (_backend == nullptr) return;
    uint32_t id;
    uint8_t  data[8];
    uint8_t  len;

    while (_backend->receive(id, data, len)) {
        // Mean Well UHP sends telemetry on 0x000C0380 + Address
        if (id == CanBusConfig::ID_PSU_READ && len >= 2) {
            uint8_t offset = 0;
            // Echoed command code?
            if (data[0] == CanBusConfig::CMD_READ_VOUT || data[0] == 0x8B) {
                offset = 1;
            }

            if (len >= offset + 2) {
                uint16_t raw = static_cast<uint16_t>(data[offset]) | (static_cast<uint16_t>(data[offset+1]) << 8);
                _telemetry.voltage = decodeLinear11(raw);
                _telemetry.lastUpdateMs = nowMs;
                _telemetry.valid = true;
                _fault = false;
                
                Serial.print(F("PSU Telemetry RX: V=")); Serial.println(_telemetry.voltage);
            }
        } else {
             Serial.print(F("CAN: Ignored packet ID 0x")); Serial.println(id, HEX);
        }
    }
}

void CanBusManager::handleWatchdog(uint32_t nowMs) {
    if (nowMs - _telemetry.lastUpdateMs > CanBusConfig::TELEMETRY_TIMEOUT_MS) {
        if (_telemetry.valid && !_fault) {
            Serial.print(F("CRITICAL: CAN Watchdog Trip! Last RX: "));
            Serial.print(nowMs - _telemetry.lastUpdateMs);
            Serial.println(F("ms ago"));
            digitalWrite(PinMap::PIN_PSU_REMOTE, LOW);
            _fault = true;
        }
    }
}

void CanBusManager::applySlewAndTransmit(uint32_t nowMs) {
    if (_backend == nullptr) return;
    float maxDeltaPerMs = CanBusConfig::MAX_DELTA_PERCENT_PER_SEC / 1000.0f;
    uint32_t dt = nowMs - _lastSetpointMs;
    if (dt == 0) return;

    float maxStep = maxDeltaPerMs * static_cast<float>(dt);
    float error   = _targetPowerPct - _currentPowerPct;

    if (error > maxStep) error = maxStep;
    else if (error < -maxStep) error = -maxStep;

    _currentPowerPct += error;
    _lastSetpointMs = nowMs;

    float targetCurrentA = (_currentPowerPct / 100.0f) * CanBusConfig::MAX_CURRENT_A;
    uint16_t linear11Current = encodeLinear11(targetCurrentA, -3);

    uint8_t payload[3];
    payload[0] = CanBusConfig::CMD_IOUT_SET;
    payload[1] = static_cast<uint8_t>(linear11Current & 0xFF);
    payload[2] = static_cast<uint8_t>((linear11Current >> 8) & 0xFF);
    _backend->send(CanBusConfig::ID_PSU_WRITE, payload, 3);
}
