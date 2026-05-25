#include <Arduino.h>
#include "Giessanlage.h"
#include "DebouncedButton.h"

using Channel = Giessanlage::Channel;

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

constexpr int BUTTON_PUMP_1 = 11;
constexpr int BUTTON_PUMP_2 = 10;
constexpr int BUTTON_CANCEL = 1;
constexpr int PUMP_1_GPIO = 23;
constexpr int PUMP_2_GPIO = 22;
} // namespace

DebouncedButton buttonPump1([] { return digitalRead(BUTTON_PUMP_1); });
DebouncedButton buttonPump2([] { return digitalRead(BUTTON_PUMP_2); });
DebouncedButton buttonCancel([] { return digitalRead(BUTTON_CANCEL); });

const unsigned long outputRemainingWaitInterval = 60UL * 1000UL;
unsigned long outputRemainingWait = 0;

static int channelLabel(Channel ch)
{
    return static_cast<int>(ch) + 1;
}

void togglePump(Channel channel)
{
    if (anlage.isPumping(channel))
    {
        anlage.stopPump(channel);
        Serial.print("Pump ");
        Serial.print(channelLabel(channel));
        Serial.println(": off");
    }
    else
    {
        anlage.triggerPump(channel);
        Serial.print("Pump ");
        Serial.print(channelLabel(channel));
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
    Serial.print("PumpTime Ch1: ");
    Serial.print(anlage.getPumpTime(Channel::One));
    Serial.println("ms");
    Serial.print("PumpTime Ch2: ");
    Serial.print(anlage.getPumpTime(Channel::Two));
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

    digitalWrite(PUMP_1_GPIO, anlage.isPumping(Channel::One) ? PUMP_ON : PUMP_OFF);
    digitalWrite(PUMP_2_GPIO, anlage.isPumping(Channel::Two) ? PUMP_ON : PUMP_OFF);

    if (buttonPump1.poll(currentTime))
        togglePump(Channel::One);
    if (buttonPump2.poll(currentTime))
        togglePump(Channel::Two);
    if (buttonCancel.poll(currentTime))
    {
        if (anlage.stopAllPumps())
            Serial.println("Cancel: all pumps off");
    }

    if (outputRemainingWait >= outputRemainingWaitInterval)
    {
        if (!anlage.isAnyPumping())
        {
            unsigned long remainTime = anlage.getRemainingWateringInterval();
            Serial.print("Remaining until next watering: ");
            Serial.print(remainTime / 1000UL / 60UL);
            Serial.println("min");
        }
        outputRemainingWait = 0;
    }
}
