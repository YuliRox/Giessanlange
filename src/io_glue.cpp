#include "io_glue.h"

#include <Arduino.h>

#include "DebouncedButton.h"
#include "MqttEvents.h"
#include "app_state.h"
#include "mqtt_glue.h"

namespace
{
// Pin map per docs/GPIO_MAPPING.md + the schematic (ESP32-C6 rework).
constexpr int BUTTON_PUMP_1 = 11;
constexpr int BUTTON_PUMP_2 = 10;
constexpr int BUTTON_CANCEL = 1;
constexpr int PUMP_1_GPIO   = 23;
constexpr int PUMP_2_GPIO   = 22;

// Pump outputs drive N-MOSFET gates in the current ESP32-C6 design.
constexpr int PUMP_ON  = HIGH;
constexpr int PUMP_OFF = LOW;

DebouncedButton buttonPump1([] { return digitalRead(BUTTON_PUMP_1); });
DebouncedButton buttonPump2([] { return digitalRead(BUTTON_PUMP_2); });
DebouncedButton buttonCancel([] { return digitalRead(BUTTON_CANCEL); });

// Throttle for the "time until next watering" serial line.
constexpr unsigned long outputRemainingWaitInterval = 60UL * 1000UL;
unsigned long outputRemainingWait = 0;

int channelLabel(Channel ch)
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
} // namespace

void ioSetup()
{
    pinMode(BUTTON_PUMP_1, INPUT_PULLUP);
    pinMode(BUTTON_PUMP_2, INPUT_PULLUP);
    pinMode(BUTTON_CANCEL, INPUT_PULLUP);
    pinMode(PUMP_1_GPIO, OUTPUT);
    pinMode(PUMP_2_GPIO, OUTPUT);
    digitalWrite(PUMP_1_GPIO, PUMP_OFF);
    digitalWrite(PUMP_2_GPIO, PUMP_OFF);
}

void ioTick(unsigned long nowMs, unsigned long elapsedMs)
{
    outputRemainingWait += elapsedMs;

    digitalWrite(PUMP_1_GPIO, anlage.isPumping(Channel::One) ? PUMP_ON : PUMP_OFF);
    digitalWrite(PUMP_2_GPIO, anlage.isPumping(Channel::Two) ? PUMP_ON : PUMP_OFF);

    if (buttonPump1.poll(nowMs))
    {
        togglePump(Channel::One);
        if (mqttEvents != nullptr) mqttEvents->publishButton("pump1", nowMs);
    }
    if (buttonPump2.poll(nowMs))
    {
        togglePump(Channel::Two);
        if (mqttEvents != nullptr) mqttEvents->publishButton("pump2", nowMs);
    }
    if (buttonCancel.poll(nowMs))
    {
        if (anlage.stopAllPumps())
            Serial.println("Cancel: all pumps off");
        if (mqttEvents != nullptr) mqttEvents->publishButton("cancel", nowMs);
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
