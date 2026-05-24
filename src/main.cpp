#include <Arduino.h>
#include "Giessanlage.h"
#include "DebouncedButton.h"

Giessanlage anlage;
unsigned long startTime = 0UL;
unsigned long elapsedTime = 0UL;

namespace
{
constexpr int BUTTON_CLOSED = LOW;
constexpr int BUTTON_OPEN = HIGH;

// Pump outputs drive N-MOSFET gates in the current ESP32-C6 design.
constexpr int PUMP_ON = HIGH;
constexpr int PUMP_OFF = LOW;

constexpr int BUTTON_PUMP_1 = 21;
constexpr int BUTTON_PUMP_2 = 22;
constexpr int BUTTON_CANCEL = 23;
constexpr int PUMP_1_GPIO = 18;
constexpr int PUMP_2_GPIO = 19;
} // namespace

DebouncedButton buttonPump1([] { return digitalRead(BUTTON_PUMP_1); });
DebouncedButton buttonPump2([] { return digitalRead(BUTTON_PUMP_2); });
DebouncedButton buttonCancel([] { return digitalRead(BUTTON_CANCEL); });

const unsigned long outputRemainingWaitInterval = 60UL * 1000UL;
unsigned long outputRemainingWait = 0;

void togglePump(int channel)
{
    if (anlage.isPumping(channel))
    {
        anlage.stopPump(channel);
        Serial.print("Pump ");
        Serial.print(channel + 1);
        Serial.println(": off");
    }
    else
    {
        anlage.triggerPump(channel);
        Serial.print("Pump ");
        Serial.print(channel + 1);
        Serial.println(": on");
    }
}

void setup()
{
    delay(200);

    pinMode(BUTTON_PUMP_1, INPUT_PULLUP);
    pinMode(BUTTON_PUMP_2, INPUT_PULLUP);
    pinMode(BUTTON_CANCEL, INPUT_PULLUP);
    pinMode(PUMP_1_GPIO, OUTPUT);
    pinMode(PUMP_2_GPIO, OUTPUT);
    digitalWrite(PUMP_1_GPIO, PUMP_OFF);
    digitalWrite(PUMP_2_GPIO, PUMP_OFF);

    delay(800);
    Serial.begin(115200);

    startTime = millis();
    elapsedTime = startTime;

    delay(1000);

    Serial.println("Hello World Giessanlange!");
    Serial.print("PumpTime: ");
    Serial.print(anlage.getPumpTime());
    Serial.println("ms");
    Serial.print("WateringInterval: ");
    Serial.print(anlage.getWateringInterval());
    Serial.println("ms");
}

void loop()
{
    unsigned long currentTime = millis();
    elapsedTime = currentTime - startTime;

    if (elapsedTime < 100)
    {
        delay(50);
        return;
    }

    startTime = currentTime;
    outputRemainingWait += elapsedTime;

    anlage.tick(elapsedTime);

    digitalWrite(PUMP_1_GPIO, anlage.isPumping(0) ? PUMP_ON : PUMP_OFF);
    digitalWrite(PUMP_2_GPIO, anlage.isPumping(1) ? PUMP_ON : PUMP_OFF);

    if (buttonPump1.poll(currentTime))
        togglePump(0);
    if (buttonPump2.poll(currentTime))
        togglePump(1);
    if (buttonCancel.poll(currentTime))
    {
        if (anlage.stopPump())
            Serial.println("Cancel: all pumps off");
    }

    if (outputRemainingWait >= outputRemainingWaitInterval)
    {
        if (!anlage.isPumping())
        {
            unsigned long remainTime = anlage.getRemainingWateringInterval();
            Serial.print("Remaining until next watering: ");
            Serial.print(remainTime / 1000UL / 60UL);
            Serial.println("min");
        }
        outputRemainingWait = 0;
    }
}
