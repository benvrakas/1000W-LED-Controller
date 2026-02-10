#include "PMBus.h"

//PSU Class Construction
    PMBusManager::PMBusManager(uint8_t sdaPin, uint8_t sclPin, uint8_t address)
        : _address(address), _bus(nullptr) 
    {}

//PSU Functions Definition
    //Initialization
    void PMBusManager::begin(TwoWire* i2cBus) {
        _bus = i2cBus;
    }