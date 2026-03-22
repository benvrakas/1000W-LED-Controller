#include "state/SystemController.h"
#include "state/StateInit.h"
#include "config/PinMap.h"
#include "config/ThermalConfig.h"
#include "logging/ErrorLogger.h"
#include "core/SystemViewModel.h"

// Note: ISR functions are declared in their respective driver headers or HardwareBridges
// We need them for attachInterrupt
#include "drivers/Tachometers.h" 
#include "drivers/PowerButton.h"
#include "drivers/Encoder.h"

//Handler Function Implementation
void handleInitState(SystemController &sys, unsigned long currentMillis) {
    //Button interupt check

    //Define variables
    static SystemStartup startup; //Static so the compiler knows this is made once and information is retained even when we go out of handleInitState scope
    auto &data = sys.initData; 
    
    //Switch statement where we go through all init steps and verify they are safe
    switch (data.bootStep) {
        case 1: //Board pins init
            sys.context.oled.showStatus("BOOT", "CHECK: PINS");
            startup.boardPinsInit(sys);
            startup.boardPinsVerify(data.bootStep);

            //Check that our hardware system is ready, if so move to next case
            if(startup.getStepStatus(data.bootStep)) {
                data.lastStepTime = currentMillis;
                data.bootStep = 2; 
            }
            break;
        
        case 2: //Pump init
            sys.context.oled.showStatus("BOOT", "CHECK: PUMP");
            startup.pumpInit(sys);
            startup.pumpVerify(data.bootStep);

            if(startup.getStepStatus(data.bootStep)) {
                data.lastStepTime = currentMillis;
                data.bootStep = 3; 
            }
            break;

        case 3: //Fans init
            sys.context.oled.showStatus("BOOT", "CHECK: FANS");
            startup.fansInit(sys);
            startup.fansVerify(data.bootStep);

            if(startup.getStepStatus(data.bootStep)) {
                data.lastStepTime = currentMillis;
                data.bootStep = 4; 
            }
            break;

        case 4: //PSU init
            sys.context.oled.showStatus("BOOT", "CHECK: PSU");
            startup.psuInit(sys);

            if(startup.getStepStatus(data.bootStep)) {
                data.lastStepTime = currentMillis;
                data.bootStep = 5; 
            }
            break;

        case 5: //Display init
            sys.context.oled.showStatus("BOOT", "CHECK: UI");
            startup.displayInit(sys);

            if(startup.getStepStatus(data.bootStep)) {
                data.lastStepTime = currentMillis;
                data.bootStep = 6; 
            }
            break;

        case 6: //Final check

            if (data.systemReady) {
                sys.transitionTo(SystemState::RUN);
            }
            break;

        default: //Illegal boot step or error got flagged, assume error
            FaultManager::instance().raiseFault(FaultCode::INIT_FAILED);
            sys.context.neoPixel.setBlinkColor(0x0000FF); // Blue for init fail
            sys.context.neoPixel.activateErrorCode(data.bootStep); // Use bootStep as error code
            ErrorLogger::instance().begin();
            {
                SystemViewModel vm = {};
                ErrorLogger::instance().update(sys, vm, currentMillis);
            }
            sys.transitionTo(SystemState::ERROR_KILL);
            return;
    }

    //Local Logic Watchdog
    if (currentMillis - data.lastStepTime > 500) {
        FaultManager::instance().raiseFault(FaultCode::INIT_FAILED);
        sys.context.neoPixel.setBlinkColor(0x0000FF); // Blue for init fail
        sys.context.neoPixel.activateErrorCode(data.bootStep); // Use bootStep as error code
        ErrorLogger::instance().begin();
        {
            SystemViewModel vm = {};
            ErrorLogger::instance().update(sys, vm, currentMillis);
        }
        sys.transitionTo(SystemState::ERROR_KILL);
        return;
    }
}

//Start up class definition 
SystemStartup::SystemStartup() 
    : _boardPinsReady(false), _pumpReady(false), _fansReady(false),
      _psuReady(false), _displayReady(false), _encoderReady(false),
      _thermistorsReady(false),
      _pumpInitDone(false), _fansInitDone(false), _psuInitDone(false)
{}

//Function definitions 
//Getters and  Setters
bool SystemStartup::getStepStatus(uint8_t bootStep) const{
    switch (bootStep) {
        case 1:  return _boardPinsReady;
        case 2:  return _pumpReady;
        case 3:  return _fansReady;
        case 4:  return _psuReady;
        case 5:  return _displayReady;
        default: return false; //log it, Watchdog for logic will kill the proccess
    }
}

void SystemStartup::setStepStatus(uint8_t bootStep, bool status) {
    switch (bootStep) {
        case 1: _boardPinsReady = status;    break;
        case 2: _pumpReady = status;         break;
        case 3: _fansReady = status;         break;
        case 4: _psuReady = status;          break;
        case 5: _displayReady = status;      break;
        default: break; //log it, Watchdog for logic will kill the proccess
    }
}

//Initialization Functions
//Board pins
void SystemStartup::boardPinsInit(SystemController& sys) {
    // Encoder Sensor Setup
    pinMode(PinMap::PIN_ENCODER_A, INPUT_PULLUP);
    pinMode(PinMap::PIN_ENCODER_B, INPUT_PULLUP);

    // Initialize encoder state once pins are configured
    sys.context.encoder.begin();

    // Power Button Setup
    pinMode(PinMap::PIN_SW_BTN, INPUT_PULLUP);
    pinMode(PinMap::PIN_SW_LED, OUTPUT);
    digitalWrite(PinMap::PIN_SW_LED, LOW); // Start with LED off

    // Thermal Sensor Setup
    pinMode(PinMap::PIN_THERM_LED, INPUT);
    pinMode(PinMap::PIN_THERM_WATER, INPUT);

    // PWM Outputs - Force LOW immediately for safety
    pinMode(PinMap::PIN_RAD_FANS_PWM, OUTPUT);
    digitalWrite(PinMap::PIN_RAD_FANS_PWM, LOW);
    
    pinMode(PinMap::PIN_PUMP_PWM, OUTPUT);
    digitalWrite(PinMap::PIN_PUMP_PWM, LOW);
    
    pinMode(PinMap::PIN_PSU_FAN_PWM, OUTPUT);
    digitalWrite(PinMap::PIN_PSU_FAN_PWM, LOW);

    pinMode(PinMap::PIN_AUX_FAN_PWM, OUTPUT);
    digitalWrite(PinMap::PIN_AUX_FAN_PWM, LOW);

    // PSU Remote ON/OFF (via N-MOSFET on UHP-1500 remote pin)
    pinMode(PinMap::PIN_PSU_REMOTE, OUTPUT);
    digitalWrite(PinMap::PIN_PSU_REMOTE, LOW);

    // Tachometer Inputs
    pinMode(PinMap::PIN_RAD_FAN_TACH, INPUT);
    pinMode(PinMap::PIN_PUMP_TACH, INPUT);
    pinMode(PinMap::PIN_PSU_FAN_TACH, INPUT);
    pinMode(PinMap::PIN_AUX_FAN_TACH, INPUT);

    // Initialise ISRs
    isrInit();
}

// Verify the safe connection settings
void SystemStartup::boardPinsVerify(uint8_t bootStep) {
    if (
        isPinSetAsInput(PinMap::PIN_ENCODER_A) && 
        isPinSetAsInput(PinMap::PIN_ENCODER_B) &&
        isPinSetAsInput(PinMap::PIN_RAD_FAN_TACH) &&
        isPinSetAsInput(PinMap::PIN_PUMP_TACH) &&
        isPinSetAsInput(PinMap::PIN_PSU_FAN_TACH) &&
        isPinSetAsInput(PinMap::PIN_AUX_FAN_TACH) &&
        
        // Check Input + Pullup state (High = not pressed)
        (isPinSetAsInput(PinMap::PIN_SW_BTN) && digitalRead(PinMap::PIN_SW_BTN) == HIGH) &&
        
        // Check Outputs + Safety Low state
        (isPinSetAsOutput(PinMap::PIN_RAD_FANS_PWM) && digitalRead(PinMap::PIN_RAD_FANS_PWM) == LOW) &&
        (isPinSetAsOutput(PinMap::PIN_PUMP_PWM) && digitalRead(PinMap::PIN_PUMP_PWM) == LOW) &&
        (isPinSetAsOutput(PinMap::PIN_PSU_FAN_PWM) && digitalRead(PinMap::PIN_PSU_FAN_PWM) == LOW) &&
        (isPinSetAsOutput(PinMap::PIN_AUX_FAN_PWM) && digitalRead(PinMap::PIN_AUX_FAN_PWM) == LOW) &&
        
        // Thermistor Sane Range Check
        (analogRead(PinMap::PIN_THERM_WATER) > 10 && analogRead(PinMap::PIN_THERM_WATER) < 4090) &&
        (analogRead(PinMap::PIN_THERM_LED) > 10 && analogRead(PinMap::PIN_THERM_LED) < 4090) &&

        // Check Encoder current readouts
        (digitalRead(PinMap::PIN_ENCODER_A) == HIGH || digitalRead(PinMap::PIN_ENCODER_B) == HIGH)
    )   {setStepStatus(bootStep, true);}
}

    //Helper functions for board pins initialization
    //Verifies pin register direction
    bool SystemStartup::isPinSetAsOutput(uint8_t pin) const {
        if (pin >= PINS_COUNT) return false; // Safety guard

        uint32_t pinMask = 1ul << g_APinDescription[pin].ulPin;
        uint32_t port = g_APinDescription[pin].ulPort;
        
        // Logic: Output if bit is NOT 0
        return (PORT->Group[port].DIR.reg & pinMask) != 0;
    }

    bool SystemStartup::isPinSetAsInput(uint8_t pin) const {
        if (pin >= PINS_COUNT) return false; // Safety guard

        uint32_t pinMask = 1ul << g_APinDescription[pin].ulPin;
        uint32_t port = g_APinDescription[pin].ulPort;
        
        // Logic: Input if bit IS 0
        return (PORT->Group[port].DIR.reg & pinMask) == 0;
    }

void SystemStartup::isrInit() {
    //Detatch any existing ISRs to avoid conflicts during re-initialization
    detachInterrupt(digitalPinToInterrupt(PinMap::PIN_SW_BTN));

    
    // Attach ISRs after pins are configured
    attachInterrupt(digitalPinToInterrupt(PinMap::PIN_PUMP_TACH), pumpISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(PinMap::PIN_RAD_FAN_TACH), mainFanISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(PinMap::PIN_AUX_FAN_TACH), auxFanISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(PinMap::PIN_PSU_FAN_TACH), psuFanISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(PinMap::PIN_SW_BTN), powerButtonISR, CHANGE);

    // Encoder quadrature interrupts on both channels
    attachInterrupt(digitalPinToInterrupt(PinMap::PIN_ENCODER_A), encoderAISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PinMap::PIN_ENCODER_B), encoderBISR, CHANGE);

    // Set priority of EIC channels
    NVIC_SetPriority(PinMap::EIC_CHANNEL_BUTTON, PinMap::EIC_PRIORITY_BUTTON);
    NVIC_SetPriority(PinMap::EIC_CHANNEL_PUMP_TACH, PinMap::EIC_PRIORITY_TACH);
    NVIC_SetPriority(PinMap::EIC_CHANNEL_MAIN_FAN_TACH, PinMap::EIC_PRIORITY_TACH);
    NVIC_SetPriority(PinMap::EIC_CHANNEL_AUX_FAN_TACH, PinMap::EIC_PRIORITY_TACH);
    NVIC_SetPriority(PinMap::EIC_CHANNEL_PSU_FAN_TACH, PinMap::EIC_PRIORITY_TACH);
    NVIC_SetPriority(PinMap::EIC_CHANNEL_ENCODER_A, PinMap::EIC_PRIORITY_ENCODER);
    NVIC_SetPriority(PinMap::EIC_CHANNEL_ENCODER_B, PinMap::EIC_PRIORITY_ENCODER);
}

//Pump
    void SystemStartup::pumpInit(SystemController& sys) {
        if (_pumpInitDone) return;
        sys.context.pump.begin();
        // Start pump at minimum duty to prime the loop
        analogWrite(PinMap::PIN_PUMP_PWM, TachometerConfig::PUMP_DEADSTART_DUTY);
        _pumpInitDone = true;
    }

    void SystemStartup::pumpVerify(uint8_t bootStep) {
        setStepStatus(bootStep, true);
    }

//Fans
    void SystemStartup::fansInit(SystemController& sys) {
        if (_fansInitDone) return;
        sys.context.mainFan.begin();
        sys.context.psuFan.begin();
        sys.context.auxFan.begin();
        // Start fans at minimum duty for initial cooling
        analogWrite(PinMap::PIN_RAD_FANS_PWM, TachometerConfig::MAIN_PSU_DEADSTART_DUTY);
        analogWrite(PinMap::PIN_PSU_FAN_PWM, TachometerConfig::MAIN_PSU_DEADSTART_DUTY);
        analogWrite(PinMap::PIN_AUX_FAN_PWM, TachometerConfig::AUX_DEADSTART_DUTY);
        _fansInitDone = true;
    }

    void SystemStartup::fansVerify(uint8_t bootStep) {
        setStepStatus(bootStep, true);
    }

//PSU
    void SystemStartup::psuInit(SystemController& sys) {
        if (_psuInitDone) return;
        // PSU CAN initialization happens in main.cpp via initHardware() -> psu.begin()
        // Ensure PSU is disabled at startup
        pinMode(PinMap::PIN_PSU_REMOTE, OUTPUT);
        digitalWrite(PinMap::PIN_PSU_REMOTE, LOW);
        _psuInitDone = true;
        setStepStatus(4, true);  // Mark PSU init complete
    }

    void SystemStartup::psuVerify(uint8_t bootStep) {
        // Verify PSU CAN communication is working
        // For now, mark as ready - actual CAN verification requires backend
        setStepStatus(bootStep, true);
    }

//Display
    void SystemStartup::displayInit(SystemController& sys) {
        // OLED initialization happens in main.cpp via initHardware() -> oled.begin()
        // Show boot screen
        sys.context.oled.showBootScreen("2.0");
        setStepStatus(5, true);  // Mark display init complete
    }

    void SystemStartup::displayVerify(uint8_t bootStep) {
        // Display verification - assume success for now
        setStepStatus(bootStep, true);
    }
