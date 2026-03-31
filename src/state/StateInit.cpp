#include "state/SystemController.h"
#include "state/StateInit.h"
#include "config/PinMap.h"
#include "config/ThermalConfig.h"
#include "logging/FaultManager.h"
#include "core/SystemViewModel.h"
#include "drivers/Tachometers.h" 
#include "drivers/PowerButton.h"
#include "drivers/Encoder.h"

void handleInitState(SystemController &sys, unsigned long currentMillis) {
    static SystemStartup startup; 
    auto &data = sys.initData; 

    if (data.bootStep == 0) {
        data.bootStep = 1;
        data.lastStepTime = currentMillis;
    }
    
    switch (data.bootStep) {
        case 1: //Board pins
            startup.boardPinsInit(sys);
            startup.boardPinsVerify(data.bootStep);
            if(startup.getStepStatus(data.bootStep)) {
                data.lastStepTime = currentMillis;
                data.bootStep = 2; 
            }
            break;
        
        case 2: //Pump
            startup.pumpInit(sys);
            startup.pumpVerify(data.bootStep);
            if(startup.getStepStatus(data.bootStep)) {
                data.lastStepTime = currentMillis;
                data.bootStep = 3; 
            }
            break;

        case 3: //Fans
            startup.fansInit(sys);
            startup.fansVerify(data.bootStep);
            if(startup.getStepStatus(data.bootStep)) {
                data.lastStepTime = currentMillis;
                data.bootStep = 4; 
            }
            break;

        case 4: //PSU
            startup.psuInit(sys);
            if(startup.getStepStatus(data.bootStep)) {
                data.lastStepTime = currentMillis;
                data.bootStep = 5; 
            }
            break;

        case 5: //Display
            startup.displayInit(sys);
            if(startup.getStepStatus(data.bootStep)) {
                data.lastStepTime = currentMillis;
                data.bootStep = 6; 
                data.systemReady = true;
            }
            break;

        case 6: //RUN transition
            if (data.systemReady) {
                sys.transitionTo(SystemState::RUN);
            }
            break;

        default:
            FaultManager::instance().raiseFault(FaultCode::INIT_FAILED);
            sys.context.neoPixel.setBlinkColor(0x0000FF);
            sys.context.neoPixel.activateErrorCode(100 + data.bootStep); 
            sys.transitionTo(SystemState::ERROR_KILL);
            return;
    }

    if (currentMillis - data.lastStepTime > 5000) {
        Serial.print(F("INIT: FATAL - Timeout Step ")); Serial.println(data.bootStep);
        FaultManager::instance().raiseFault(FaultCode::INIT_FAILED);
        sys.context.neoPixel.setBlinkColor(0x0000FF);
        sys.context.neoPixel.activateErrorCode(100 + data.bootStep); 
        sys.transitionTo(SystemState::ERROR_KILL);
        return;
    }
}

SystemStartup::SystemStartup() 
    : _boardPinsReady(false), _pumpReady(false), _fansReady(false),
      _psuReady(false), _displayReady(false), _encoderReady(false),
      _thermistorsReady(false),
      _pumpInitDone(false), _fansInitDone(false), _psuInitDone(false)
{}

bool SystemStartup::getStepStatus(uint8_t bootStep) const {
    switch (bootStep) {
        case 1:  return _boardPinsReady;
        case 2:  return _pumpReady;
        case 3:  return _fansReady;
        case 4:  return _psuReady;
        case 5:  return _displayReady;
        default: return false; 
    }
}

void SystemStartup::setStepStatus(uint8_t bootStep, bool status) {
    switch (bootStep) {
        case 1: _boardPinsReady = status;    break;
        case 2: _pumpReady = status;         break;
        case 3: _fansReady = status;         break;
        case 4: _psuReady = status;          break;
        case 5: _displayReady = status;      break;
        default: break; 
    }
}

void SystemStartup::boardPinsInit(SystemController& sys) {
    pinMode(PinMap::PIN_ENCODER_A, INPUT_PULLUP);
    pinMode(PinMap::PIN_ENCODER_B, INPUT_PULLUP);
    sys.context.encoder.begin();
    pinMode(PinMap::PIN_SW_BTN, INPUT_PULLUP);
    pinMode(PinMap::PIN_SW_LED, OUTPUT);
    digitalWrite(PinMap::PIN_SW_LED, LOW);
    pinMode(PinMap::PIN_THERM_LED, INPUT);
    pinMode(PinMap::PIN_THERM_WATER, INPUT);
    pinMode(PinMap::PIN_RAD_FANS_PWM, OUTPUT);
    digitalWrite(PinMap::PIN_RAD_FANS_PWM, LOW);
    pinMode(PinMap::PIN_PUMP_PWM, OUTPUT);
    digitalWrite(PinMap::PIN_PUMP_PWM, LOW);
    pinMode(PinMap::PIN_PSU_FAN_PWM, OUTPUT);
    digitalWrite(PinMap::PIN_PSU_FAN_PWM, LOW);
    pinMode(PinMap::PIN_AUX_FAN_PWM, OUTPUT);
    digitalWrite(PinMap::PIN_AUX_FAN_PWM, LOW);
    pinMode(PinMap::PIN_PSU_REMOTE, OUTPUT);
    digitalWrite(PinMap::PIN_PSU_REMOTE, LOW);
    pinMode(PinMap::PIN_RAD_FAN_TACH, INPUT);
    pinMode(PinMap::PIN_PUMP_TACH, INPUT);
    pinMode(PinMap::PIN_PSU_FAN_TACH, INPUT);
    pinMode(PinMap::PIN_AUX_FAN_TACH, INPUT);
    isrInit();
}

void SystemStartup::boardPinsVerify(uint8_t bootStep) {
    bool ok = true;
    int water = analogRead(PinMap::PIN_THERM_WATER);
    int led = analogRead(PinMap::PIN_THERM_LED);
    if (water < 10 || water > 4085) ok = false;
    if (led < 10 || led > 4085) ok = false;
    if (ok) setStepStatus(bootStep, true);
}

bool SystemStartup::isPinSetAsOutput(uint8_t pin) const {
    if (pin >= PINS_COUNT) return false; 
    uint32_t pinMask = 1ul << g_APinDescription[pin].ulPin;
    uint32_t port = g_APinDescription[pin].ulPort;
    return (PORT->Group[port].DIR.reg & pinMask) != 0;
}

bool SystemStartup::isPinSetAsInput(uint8_t pin) const {
    if (pin >= PINS_COUNT) return false; 
    uint32_t pinMask = 1ul << g_APinDescription[pin].ulPin;
    uint32_t port = g_APinDescription[pin].ulPort;
    return (PORT->Group[port].DIR.reg & pinMask) == 0;
}

void SystemStartup::isrInit() {
    detachInterrupt(digitalPinToInterrupt(PinMap::PIN_SW_BTN));
    attachInterrupt(digitalPinToInterrupt(PinMap::PIN_PUMP_TACH), pumpISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(PinMap::PIN_RAD_FAN_TACH), mainFanISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(PinMap::PIN_AUX_FAN_TACH), auxFanISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(PinMap::PIN_PSU_FAN_TACH), psuFanISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(PinMap::PIN_SW_BTN), powerButtonISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PinMap::PIN_ENCODER_A), encoderAISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PinMap::PIN_ENCODER_B), encoderBISR, CHANGE);
}

void SystemStartup::pumpInit(SystemController& sys) {
    if (_pumpInitDone) return;
    sys.context.pump.begin();
    analogWrite(PinMap::PIN_PUMP_PWM, TachometerConfig::PUMP_DEADSTART_DUTY);
    _pumpInitDone = true;
}

void SystemStartup::pumpVerify(uint8_t bootStep) { setStepStatus(bootStep, true); }

void SystemStartup::fansInit(SystemController& sys) {
    if (_fansInitDone) return;
    sys.context.mainFan.begin();
    sys.context.psuFan.begin();
    sys.context.auxFan.begin();
    analogWrite(PinMap::PIN_RAD_FANS_PWM, TachometerConfig::MAIN_PSU_DEADSTART_DUTY);
    analogWrite(PinMap::PIN_PSU_FAN_PWM, TachometerConfig::MAIN_PSU_DEADSTART_DUTY);
    analogWrite(PinMap::PIN_AUX_FAN_PWM, TachometerConfig::AUX_DEADSTART_DUTY);
    _fansInitDone = true;
}

void SystemStartup::fansVerify(uint8_t bootStep) { setStepStatus(bootStep, true); }

void SystemStartup::psuInit(SystemController& sys) {
    if (_psuInitDone) return;
    pinMode(PinMap::PIN_PSU_REMOTE, OUTPUT);
    digitalWrite(PinMap::PIN_PSU_REMOTE, LOW);
    _psuInitDone = true;
    setStepStatus(4, true); 
}

void SystemStartup::psuVerify(uint8_t bootStep) { setStepStatus(bootStep, true); }

void SystemStartup::displayInit(SystemController& sys) {
    sys.context.oled.showBootScreen("2.0");
    setStepStatus(5, true); 
}

void SystemStartup::displayVerify(uint8_t bootStep) { setStepStatus(bootStep, true); }
