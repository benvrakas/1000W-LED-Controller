 #include "OLED.h"
 
 //Display Class Construction
    OledManager::OledManager(uint8_t sdaPin, uint8_t sclPin, uint8_t address, uint8_t width, uint8_t height)
        : _address(address), _width(width), _height(height), _bus(nullptr), _display(nullptr) 
    {}

    //Display Functions Definition
        //Initialization
        void OledManager::begin(TwoWire* i2cBus) {
            _bus = i2cBus;
            _display = new Adafruit_SSD1306(_width, _height, _bus, -1);

        }