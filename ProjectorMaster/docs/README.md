#Projector Controller 

Important Notes:
    All essential systems need to be non-blocking and all systems must not
    inturupt essential systems. We can't have a situation where a task is taking up time from an essential proccess. For example if a oled object or function blocks thermistor read for 500ms that is enough time to damage hardware

File Structure:

Main Logic Structure:
    Interupt - on seperate hardware //All Interupt data has to be stored as a volatile
        Encoder - Interupt
            300 pulse
        Power button presses - Interupt
        Tachocometer Readings - Interupt
    Fast Systems
        PM Bus - Milis(50)
            Current controlled - Max = 20.8333
    Slow Systems
        Main Fan PWM - Milis(100)
        Aux Fan PWM - Milis(100)
        LED Housing Fan PWM - Milis(100)
        Thermistor Reading - Millis(100)
            50c for water
            75 for led
        Oled updates
        PID Tuning


        /**
 * THE "NON-BLOCKING" RULESET:
 * * 1. NO DELAYS: Never use delay() in any function.
 * 2. NO UNBOUNDED WHILES: Never use while(condition) unless you have 
 * a secondary timeout check inside the while loop.
 * 3. FAST PATH vs SLOW PATH: 
 * - Safety (Tachs/Killswitch) = FAST PATH (Runs every loop)
 * - UI/Sensors = SLOW PATH (Runs on timers)
 */
