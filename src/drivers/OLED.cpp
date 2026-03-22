 #include "drivers/OLED.h"
 
 //Display Class Construction
    OledManager::OledManager(uint8_t sdaPin, uint8_t sclPin, uint8_t address, uint8_t width, uint8_t height)
        : _address(address), _width(width), _height(height), _bus(nullptr), _display(nullptr) 
    {}

 //Display Functions Definition
 //Initialization
     void OledManager::begin(TwoWire* i2cBus) {
         _bus = i2cBus;
         _display = new Adafruit_SSD1306(_width, _height, _bus, -1);
         
         if(_display->begin(SSD1306_SWITCHCAPVCC, _address)) {
             _display->clearDisplay();
             _display->display();
         }
     }

     void OledManager::showBootScreen(const char* version) {
         if (!_display) return;
         _display->clearDisplay();
         _display->setTextColor(SSD1306_WHITE);
         
         // Title
         _display->setTextSize(2);
         _display->setCursor(10, 0);
         _display->print(F("PROJECTOR"));
         
         // Version
         _display->setTextSize(1);
         _display->setCursor(10, 20);
         _display->print(F("v"));
         _display->print(version);
         
         _display->display();
     }

     void OledManager::updateTelemetry(float voltage, float current, float temp) {
         if (!_display) return;
         _display->clearDisplay();
         _display->setTextSize(1);
         _display->setTextColor(SSD1306_WHITE);
         _display->setCursor(0, 0);
         
         _display->print(F("V: ")); _display->print(voltage, 1);
         _display->print(F("  A: ")); _display->println(current, 1);
         _display->print(F("T: ")); _display->print(temp, 1); _display->println(F(" C"));
         
         _display->display();
     }

     void OledManager::showStatus(const char* title, const char* msg) {
         if (!_display) return;
         _display->clearDisplay();
         _display->setTextSize(2);
         _display->setTextColor(SSD1306_WHITE);
         _display->setCursor(0, 0);
         _display->println(title);
         _display->setTextSize(1);
         _display->println(msg);
         _display->display();
     }

     void OledManager::showError(const char* msg) {
         if (!_display) return;
         _display->clearDisplay();
         _display->setTextSize(2); // Large text for error
         _display->setTextColor(SSD1306_WHITE);
         _display->setCursor(0, 0);
         _display->println(F("ERROR!"));
         _display->setTextSize(1);
         _display->println(msg);
         _display->display();
     }
