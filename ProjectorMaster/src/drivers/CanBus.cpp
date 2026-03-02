#include "CanBus.h"
#include "util/BoardPins.h"

CanBusManager::CanBusManager(uint8_t txPin, uint8_t rxPin)
    : _txPin(txPin), _rxPin(rxPin), _backend(nullptr),
      _targetPowerPct(0.0f), _currentPowerPct(0.0f), _lastSetpointMs(0),
      _telemetry{0.0f, 0.0f, 0.0f, 0, false}, _fault(false)
{}

void CanBusManager::begin(ICanBackend* backend) {
    _backend = backend;

    // Ensure PSU enable is defined and starts in a safe (off) state
    pinMode(BoardPins::PIN_PSU_ENABLE, OUTPUT);
    digitalWrite(BoardPins::PIN_PSU_ENABLE, LOW);

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

    processIncoming(nowMs);
    handleWatchdog(nowMs);
    applySlewAndTransmit(nowMs);
}

void CanBusManager::setOperation(bool enable) {
    if (_backend == nullptr) return;

    // Simple 1-byte payload: 0 = off, 1 = on
    uint8_t data[1];
    data[0] = enable ? 1u : 0u;
    _backend->send(CanBusConfig::ID_OPERATION, data, 1);

    // Hardware kill line should never be high when we command off
    if (!enable) {
        digitalWrite(BoardPins::PIN_PSU_ENABLE, LOW);
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
        if (id == CanBusConfig::ID_TELEMETRY && len >= 3) {
            // NOTE: Actual encoding is PSU-specific. For now, treat the first
            // 3 bytes as coarse voltage/current/temp percentages and mark
            // telemetry as present. The exact mapping can be refined later.
            _telemetry.voltage     = static_cast<float>(data[0]);
            _telemetry.current     = static_cast<float>(data[1]);
            _telemetry.temperature = static_cast<float>(data[2]);
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
        digitalWrite(BoardPins::PIN_PSU_ENABLE, LOW);
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

    // Map 0–100% to 0–255 (single-byte setpoint). Exact encoding can be
    // replaced with the PSU's documented format later.
    uint8_t level = static_cast<uint8_t>((_currentPowerPct / 100.0f) * 255.0f + 0.5f);

    uint8_t payload[1];
    payload[0] = level;
    _backend->send(CanBusConfig::ID_SET_CURRENT, payload, 1);
}

