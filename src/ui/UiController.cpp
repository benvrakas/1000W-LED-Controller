#include "ui/UiController.h"
#include "drivers/OLED.h"

UiController::UiController(OledManager& oled) : _oled(oled) {}
void UiController::begin() {}

static void drawGauge(Adafruit_SH1107* d, int16_t cx, int16_t cy, int16_t radius, float fraction) {
    if (!d) return;
    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;
    const float startDeg = -120.0f;
    const float endDeg   =  120.0f;
    float sweepDeg       = (endDeg - startDeg) * fraction;
    for (float deg = startDeg; deg <= startDeg + sweepDeg; deg += 6.0f) {
        float rad = deg * 3.14159265f / 180.0f;
        int16_t x = static_cast<int16_t>(cx + radius * cosf(rad));
        int16_t y = static_cast<int16_t>(cy + radius * sinf(rad));
        d->drawPixel(x, y, SH110X_WHITE);
    }
}

void UiController::update(const SystemViewModel& vm, unsigned long now) {
    if (!_oled.isReady()) return;
    Adafruit_SH1107* d = _oled.getDisplay();
    if (!d) return;

    d->clearDisplay();
    d->setTextSize(1);
    d->setTextColor(SH110X_WHITE);
    d->setCursor(0, 0);

    d->print(F("V:"));   d->print(vm.psuVoltage, 1);
    d->print(F(" A:"));  d->print(vm.psuCurrent, 1);
    d->print(F(" P:"));  d->println(vm.psuPower, 0);

    d->setCursor(0, 12);
    d->print(F("LED:"));   d->print(vm.ledTempC, 1);
    d->print(F("C W:"));   d->print(vm.waterTempC, 1);
    d->println(F("C"));

    d->setCursor(0, 24);
    d->print(F("FANS: M:")); d->print(vm.mainFanRPM);
    d->print(F(" A:")); d->println(vm.auxFanRPM);

    d->setCursor(0, 36);
    d->print(F("PUMP: ")); d->print(vm.pumpRPM);
    d->print(F(" PSU: ")); d->println(vm.psuFanRPM);

    d->setCursor(0, 52);
    if (vm.isArmed) {
        // Error warning if armed but PSU is not reporting voltage
        if (vm.psuVoltage < 1.0f) {
            d->print(F("[NO PSU COMM!]"));
        } else {
            d->print(F("[POWER ON]"));
        }
    } else {
        d->print(F("[IDLE]"));
    }

    const int16_t centerX = 110;
    const int16_t radius  = 12;
    drawGauge(d, centerX, 18, radius, vm.knobFraction);
    d->setCursor(centerX - 3, 32); d->print(F("K"));
    drawGauge(d, centerX, 48, radius, vm.appliedFraction);
    d->setCursor(centerX - 3, 62); d->print(F("L"));

    d->display();
}
