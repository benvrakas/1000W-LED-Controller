#include "drivers/NativeCanBackend.h"
#include "config/PinMap.h"
#include <Arduino.h>

NativeCanBackend::NativeCanBackend() : _ready(false) {}

bool NativeCanBackend::begin(uint32_t bitrate) {
    // Feather M4 CAN Express uses internal CAN transceiver with built-in standby and boost
    pinMode(PIN_CAN_STANDBY, OUTPUT);
    digitalWrite(PIN_CAN_STANDBY, LOW); // LOW means active
    pinMode(PIN_CAN_BOOSTEN, OUTPUT);
    digitalWrite(PIN_CAN_BOOSTEN, HIGH); // HIGH means boost enabled

    _ready = CAN.begin(bitrate);
    
    if (_ready) {
        Serial.print(F("CAN: Initialized OK at "));
        Serial.print(bitrate / 1000);
        Serial.println(F(" kbps"));
    } else {
        Serial.println(F("CAN: Init FAILED - CAN operations disabled"));
    }
    
    return _ready;
}

bool NativeCanBackend::send(uint32_t id, const uint8_t* data, uint8_t len) {
    if (!_ready) return false;
    
    // Log attempt
    Serial.print(F("CAN TX: 0x"));
    Serial.print(id, HEX);
    Serial.print(F(" ["));
    Serial.print(len);
    Serial.print(F("] "));
    for(int i=0; i<len; i++) { Serial.print(data[i], HEX); Serial.print(" "); }

    // Mean Well uses extended 29-bit IDs
    if (!CAN.beginExtendedPacket(id)) {
        Serial.println(F(" -> beginExtendedPacket FAILED"));
        return false;
    }
    
    // Only write data if we actually have a payload
    if (data != nullptr && len > 0) {
        size_t bytesWritten = CAN.write(data, len);
        if (bytesWritten != len) {
            Serial.println(F(" -> Write FAILED"));
            return false;
        }
    }
    
    bool ok = CAN.endPacket();
    if (!ok) {
        Serial.println(F(" -> endPacket FAILED"));
    } else {
        Serial.println(F(" -> OK"));
    }
    return ok;
}

bool NativeCanBackend::receive(uint32_t& id, uint8_t* data, uint8_t& len) {
    if (!_ready) return false;
    
    int packetSize = CAN.parsePacket();
    
    if (packetSize) {
        id = CAN.packetId();
        len = (uint8_t)packetSize;
        if (len > 8) len = 8;
        
        Serial.print(F("CAN RX: 0x"));
        Serial.print(id, HEX);
        Serial.print(F(" ["));
        Serial.print(len);
        Serial.print(F("] "));

        // Read data into buffer
        int bytesRead = 0;
        while (CAN.available() && bytesRead < len) {
            uint8_t b = CAN.read();
            data[bytesRead++] = b;
            Serial.print(b, HEX);
            Serial.print(" ");
        }
        Serial.println();
        return true;
    }
    return false;
}
