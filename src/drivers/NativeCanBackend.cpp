#include "drivers/NativeCanBackend.h"
#include "config/PinMap.h"

NativeCanBackend::NativeCanBackend() {}

bool NativeCanBackend::begin(uint32_t bitrate) {
    // Feather M4 CAN Express uses internal CAN transceiver with built-in standby and boost
    // Need to initialize these pins for the CAN transceiver to operate
    pinMode(PIN_CAN_STANDBY, OUTPUT);
    digitalWrite(PIN_CAN_STANDBY, LOW); // LOW means active
    pinMode(PIN_CAN_BOOSTEN, OUTPUT);
    digitalWrite(PIN_CAN_BOOSTEN, HIGH); // HIGH means boost enabled

    return CAN.begin(bitrate);
}

bool NativeCanBackend::send(uint32_t id, const uint8_t* data, uint8_t len) {
    // Mean Well uses extended 29-bit IDs
    if (CAN.beginExtendedPacket(id)) {
        size_t bytesWritten = CAN.write(data, len);
        return (bytesWritten == len) && CAN.endPacket();
    }
    return false;
}

bool NativeCanBackend::receive(uint32_t& id, uint8_t* data, uint8_t& len) {
    int packetSize = CAN.parsePacket();
    
    if (packetSize) {
        id = CAN.packetId();
        len = (uint8_t)packetSize;
        if (len > 8) len = 8;
        
        // Read data into buffer
        int bytesRead = 0;
        while (CAN.available() && bytesRead < len) {
            data[bytesRead++] = CAN.read();
        }
        return true;
    }
    return false;
}
