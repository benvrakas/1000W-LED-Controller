#include "ui/UiController.h"

#include "drivers/OLED.h"

UiController::UiController(OledManager& oled) : _oled(oled) {}

void UiController::begin() {
    // Boot screen is handled by StateInit during system startup sequence.
}

// Simple helper to draw a circular gauge (arc) representing a 0..1 value.
static void drawGauge(Adafruit_SSD1306* d,
                      int16_t cx, int16_t cy,
                      int16_t radius,
                      float   fraction) {
    if (!d) return;
    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;

    // Sweep arc from -120 to +120 degrees for better use of the small 32px
    // vertical space on the 128x32 OLED.
    const float startDeg = -120.0f;
    const float endDeg   =  120.0f;
    float sweepDeg       = (endDeg - startDeg) * fraction;

    for (float deg = startDeg; deg <= startDeg + sweepDeg; deg += 6.0f) {
        float rad = deg * 3.14159265f / 180.0f;
        int16_t x = static_cast<int16_t>(cx + radius * cosf(rad));
        int16_t y = static_cast<int16_t>(cy + radius * sinf(rad));
        d->drawPixel(x, y, SSD1306_WHITE);
    }
}

void UiController::update(const SystemViewModel& vm, unsigned long now) {
    (void)now;

    if (!_oled.isReady()) return;

    Adafruit_SSD1306* d = _oled.getDisplay();
    if (!d) return;

    // Clear screen
    d->clearDisplay();
    d->setTextSize(1);
    d->setTextColor(SSD1306_WHITE);
    d->setCursor(0, 0);

    // Line 0: PSU stats (V, A, W)
    d->print(F("V:"));   d->print(vm.psuVoltage, 1);
    d->print(F(" A:"));  d->print(vm.psuCurrent, 1);
    d->print(F(" P:"));  d->print(vm.psuPower, 0);

    // Line 1: Temps
    d->setCursor(0, 8);
    d->print(F("LED:"));   d->print(vm.ledTempC, 1);
    d->print(F("C W:"));   d->print(vm.waterTempC, 1);
    d->print(F("C"));

    // Line 2: Fans/Pump RPM (main + pump)
    d->setCursor(0, 16);
    d->print(F("M:")); d->print(vm.mainFanRPM);
    d->print(F(" P:")); d->print(vm.pumpRPM);

    // Line 3: Aux + PSU fan RPM
    d->setCursor(0, 24);
    d->print(F("A:")); d->print(vm.auxFanRPM);
    d->print(F(" F:")); d->print(vm.psuFanRPM);

    // Two small gauges at the right side
    const int16_t centerX = 100;
    const int16_t centerY1 = 10;  // virtual knob
    const int16_t centerY2 = 22;  // LED setpoint (applied)
    const int16_t radius  = 7;

    // Virtual encoder knob gauge
    drawGauge(d, centerX, centerY1, radius, vm.knobFraction);
    d->setCursor(centerX + 10, centerY1 - 3);
    d->print(F("K")); // label

    // LED applied setpoint gauge
    drawGauge(d, centerX, centerY2, radius, vm.appliedFraction);
    d->setCursor(centerX + 10, centerY2 - 3);
    d->print(F("L")); // label

    d->display();
}


