#include "state/SystemController.h"
#include "state/StateInit.h"
#include "util/BoardPins.h"
#include "config/FanCurves.h"

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
    auto &data = sys.stateData.init; 
    
    //Switch statement where we go through all init steps and verify they are safe
    switch (data.bootStep) {
        case 1: //Board pins init
            startup.boardPinsInit(sys);
            startup.boardPinsVerify(data.bootStep);

            //Check that our hardware system is ready, if so move to next case
            if(startup.getStepStatus(data.bootStep)) {
                data.lastStepTime = currentMillis;
                data.bootStep = 2; 
            }
            break;
        
        case 2: //Pump init
            startup.pumpInit(sys);

            if(startup.getStepStatus(data.bootStep)) {
                data.lastStepTime = currentMillis;
                data.bootStep = 3; 
            }
            break;

        case 3: //Fans init
            startup.fansInit(sys);

            if(startup.getStepStatus(data.bootStep)) {
                data.lastStepTime = currentMillis;
                data.bootStep = 4; 
            }
            break;

        case 4: //PSU init
            startup.psuInit(sys);

            if(startup.getStepStatus(data.bootStep)) {
                data.lastStepTime = currentMillis;
                data.bootStep = 5; 
            }
            break;

        case 5: //Display init
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
            sys.transitionTo(SystemState::ERROR_KILL);
            return;
    }

    //Local Logic Watchdog
    if (currentMillis - data.lastStepTime > 500) {
        sys.transitionTo(SystemState::ERROR_KILL);
        return;
    }
}

//Start up class definition 
SystemStartup::SystemStartup() 
    : _boardPinsReady(false), _pumpReady(false), _fansReady(false),
      _psuReady(false), _displayReady(false), _encoderReady(false),
      _thermistorsReady(false) 
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
    pinMode(BoardPins::PIN_ENCODER_A, INPUT_PULLUP);
    pinMode(BoardPins::PIN_ENCODER_B, INPUT_PULLUP);

    // Initialize encoder state once pins are configured
    sys.context.encoder.begin();

    // Power Button Setup
    pinMode(BoardPins::PIN_SW_BTN, INPUT_PULLUP);
    pinMode(BoardPins::PIN_SW_LED, OUTPUT);
    digitalWrite(BoardPins::PIN_SW_LED, LOW); // Start with LED off

    // Thermal Sensor Setup
    pinMode(BoardPins::PIN_THERM_LED, INPUT);
    pinMode(BoardPins::PIN_THERM_WATER, INPUT);

    // PWM Outputs - Force LOW immediately for safety
    pinMode(BoardPins::PIN_RAD_FANS_PWM, OUTPUT);
    digitalWrite(BoardPins::PIN_RAD_FANS_PWM, LOW);
    
    pinMode(BoardPins::PIN_PUMP_PWM, OUTPUT);
    digitalWrite(BoardPins::PIN_PUMP_PWM, LOW);
    
    pinMode(BoardPins::PIN_PSU_FAN_PWM, OUTPUT);
    digitalWrite(BoardPins::PIN_PSU_FAN_PWM, LOW);

    pinMode(BoardPins::PIN_AUX_FAN_PWM, OUTPUT);
    digitalWrite(BoardPins::PIN_AUX_FAN_PWM, LOW);

    // PSU Remote ON/OFF (via N-MOSFET on UHP-1500 remote pin)
    pinMode(BoardPins::PIN_PSU_ENABLE, OUTPUT);
    digitalWrite(BoardPins::PIN_PSU_ENABLE, LOW);

    pinMode(BoardPins::PIN_PSU_REMOTE, OUTPUT);
    digitalWrite(BoardPins::PIN_PSU_REMOTE, LOW);

    // Tachometer Inputs
    pinMode(BoardPins::PIN_RAD_FAN_TACH, INPUT);
    pinMode(BoardPins::PIN_PUMP_TACH, INPUT);
    pinMode(BoardPins::PIN_PSU_FAN_TACH, INPUT);
    pinMode(BoardPins::PIN_AUX_FAN_TACH, INPUT);

    // Initialise ISRs
    isrInit();
}

// Verify the safe connection settings
void SystemStartup::boardPinsVerify(uint8_t bootStep) {
    if (
        isPinSetAsInput(BoardPins::PIN_ENCODER_A) && 
        isPinSetAsInput(BoardPins::PIN_ENCODER_B) &&
        isPinSetAsInput(BoardPins::PIN_RAD_FAN_TACH) &&
        isPinSetAsInput(BoardPins::PIN_PUMP_TACH) &&
        isPinSetAsInput(BoardPins::PIN_PSU_FAN_TACH) &&
        isPinSetAsInput(BoardPins::PIN_AUX_FAN_TACH) &&
        
        // Check Input + Pullup state (High = not pressed)
        (isPinSetAsInput(BoardPins::PIN_SW_BTN) && digitalRead(BoardPins::PIN_SW_BTN) == HIGH) &&
        
        // Check Outputs + Safety Low state
        (isPinSetAsOutput(BoardPins::PIN_RAD_FANS_PWM) && digitalRead(BoardPins::PIN_RAD_FANS_PWM) == LOW) &&
        (isPinSetAsOutput(BoardPins::PIN_PUMP_PWM) && digitalRead(BoardPins::PIN_PUMP_PWM) == LOW) &&
        (isPinSetAsOutput(BoardPins::PIN_PSU_FAN_PWM) && digitalRead(BoardPins::PIN_PSU_FAN_PWM) == LOW) &&
        (isPinSetAsOutput(BoardPins::PIN_AUX_FAN_PWM) && digitalRead(BoardPins::PIN_AUX_FAN_PWM) == LOW) &&
        
        // Thermistor Sane Range Check
        (analogRead(BoardPins::PIN_THERM_WATER) > 10 && analogRead(BoardPins::PIN_THERM_WATER) < 4090) &&
        (analogRead(BoardPins::PIN_THERM_LED) > 10 && analogRead(BoardPins::PIN_THERM_LED) < 4090) &&

        // Check Encoder current readouts
        (digitalRead(BoardPins::PIN_ENCODER_A) == HIGH || digitalRead(BoardPins::PIN_ENCODER_B) == HIGH)
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
    detachInterrupt(digitalPinToInterrupt(BoardPins::PIN_SW_BTN));

    
    // Attach ISRs after pins are configured
    attachInterrupt(digitalPinToInterrupt(BoardPins::PIN_PUMP_TACH), pumpISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(BoardPins::PIN_RAD_FAN_TACH), mainFanISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(BoardPins::PIN_AUX_FAN_TACH), auxFanISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(BoardPins::PIN_PSU_FAN_TACH), psuFanISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(BoardPins::PIN_SW_BTN), powerButtonISR, CHANGE);

    // Encoder quadrature interrupts on both channels
    attachInterrupt(digitalPinToInterrupt(BoardPins::PIN_ENCODER_A), encoderAISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(BoardPins::PIN_ENCODER_B), encoderBISR, CHANGE);

    // Set priority of EIC channels
    NVIC_SetPriority(BoardPins::EIC_CHANNEL_BUTTON, BoardPins::EIC_PRIORITY_BUTTON);
    NVIC_SetPriority(BoardPins::EIC_CHANNEL_PUMP_TACH, BoardPins::EIC_PRIORITY_TACH);
    NVIC_SetPriority(BoardPins::EIC_CHANNEL_MAIN_FAN_TACH, BoardPins::EIC_PRIORITY_TACH);
    NVIC_SetPriority(BoardPins::EIC_CHANNEL_AUX_FAN_TACH, BoardPins::EIC_PRIORITY_TACH);
    NVIC_SetPriority(BoardPins::EIC_CHANNEL_PSU_FAN_TACH, BoardPins::EIC_PRIORITY_TACH);
    NVIC_SetPriority(BoardPins::EIC_CHANNEL_ENCODER_A, BoardPins::EIC_PRIORITY_ENCODER);
    NVIC_SetPriority(BoardPins::EIC_CHANNEL_ENCODER_B, BoardPins::EIC_PRIORITY_ENCODER);
}

//Pump
    void SystemStartup::pumpInit(SystemController& sys) {
        sys.context.pump.begin();
        // Start pump at minimum duty to prime the loop
        analogWrite(BoardPins::PIN_PUMP_PWM, TachometerConfig::PUMP_DEADSTART_DUTY);
        setStepStatus(2, true);  // Mark pump init complete
    }

    void SystemStartup::pumpVerify(uint8_t bootStep) {
        // Verify pump is spinning (RPM > stall threshold after brief delay)
        // For now, assume init succeeded - actual verification requires ISR time
        setStepStatus(bootStep, true);
    }

//Fans
    void SystemStartup::fansInit(SystemController& sys) {
        sys.context.mainFan.begin();
        sys.context.psuFan.begin();
        sys.context.auxFan.begin();
        // Start fans at minimum duty for initial cooling
        analogWrite(BoardPins::PIN_RAD_FANS_PWM, TachometerConfig::MAIN_PSU_DEADSTART_DUTY);
        analogWrite(BoardPins::PIN_PSU_FAN_PWM, TachometerConfig::MAIN_PSU_DEADSTART_DUTY);
        analogWrite(BoardPins::PIN_AUX_FAN_PWM, TachometerConfig::AUX_DEADSTART_DUTY);
        setStepStatus(3, true);  // Mark fans init complete
    }

    void SystemStartup::fansVerify(uint8_t bootStep) {
        // Verify fans are spinning - for now assume success
        setStepStatus(bootStep, true);
    }

//PSU
    void SystemStartup::psuInit(SystemController& sys) {
        // PSU CAN initialization happens in main.cpp via initHardware() -> psu.begin()
        // Ensure PSU is disabled at startup
        pinMode(BoardPins::PIN_PSU_ENABLE, OUTPUT);
        digitalWrite(BoardPins::PIN_PSU_ENABLE, LOW);
        pinMode(BoardPins::PIN_PSU_REMOTE, OUTPUT);
        digitalWrite(BoardPins::PIN_PSU_REMOTE, LOW);
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
