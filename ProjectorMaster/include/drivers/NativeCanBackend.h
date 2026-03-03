#pragma once
#include "drivers/CanBus.h"
#include <CAN.h>

class NativeCanBackend : public ICanBackend {
public:
    NativeCanBackend();
    bool begin(uint32_t bitrate) override;
    bool send(uint32_t id, const uint8_t* data, uint8_t len) override;
    bool receive(uint32_t& id, uint8_t* data, uint8_t& len) override;
};
