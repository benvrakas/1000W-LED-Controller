#pragma once

volatile bool abortRequested;

class PowerButtonManager {
    public:
        //Class Construction
        PowerButtonManager(uint8_t swPin, uint8_t ledPin);

        //Initialization
        void begin();

        //Getters
        bool isPressed() const;
        bool isLongPress() const;

        //Monitoring

        //ISR Support
        void handleButtonInterrupt();
        
    private:
        uint16_t _pressDuration;
        bool _buttonPressed;
        volatile bool _abortRequested;
        bool _lastState;

};
extern PowerButtonManager powerButton;

//ISR Bridge Function Declarations
void powerButtonISR();