#Projector Controller 

Important Notes:
    All essential systems need to be non-blocking and all systems must not
    inturupt essential systems. We can't have a situation where a task is taking up time from an essential proccess. For example if a oled object or function blocks thermistor read for 500ms that is enough time to damage hardware

File Structure:

Main Logic Structure:
    Interupt - on seperate hardware //All Interupt data has to be stored as a volatile
        Encoder - Interupt -- Rank 3
            300 pulse
        Power button presses - Interupt -- Rank 1 --- Port Pin PB09, EXTINT channel 9
        Tachocometer Pulse Readings - Interupt -- Rank 2
    Fast Systems
        PM Bus - Millis(50)
            Current controlled - Max = 20.8333
    Slow Systems
        Main Fan PWM - Millis(100) - PID Tuned, using water temp readings
        Aux Fan PWM - Millis(20) - PWM = Brightness level --Compare encoder min-max to a value that can be
            represented 0-255
        PSU Fan PWM - Millis(20) - PID Tuned, using pmbus temp readings
        Pump PWM - Millis(20) - PID Tuned, using led temp readings (EK-D5 Vario Motor 12-24V DC Pump Motor)
        Thermistor Ideal Reading
            50c for water - Millis(100)
            75 for led - Millis(20)
        Oled updates
        PID Tuning -This needs to be it's own class, not every Tach instance needs this information
            Calculate RPM
            Take Thermal reading
            Calculate PID
            Calculate PWM


        /**
 * THE "NON-BLOCKING" RULESET:
 * * 1. NO DELAYS: Never use delay() in any function.
 * 2. NO UNBOUNDED WHILES: Never use while(condition) unless you have 
 * a secondary timeout check inside the while loop.
 * 3. FAST PATH vs SLOW PATH: 
 * - Safety (Tachs/Killswitch) = FAST PATH (Runs every loop)
 * - UI/Sensors = SLOW PATH (Runs on timers)
 */
