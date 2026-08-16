#include <Arduino.h>

#include "app_state.h"
#include "wifi_glue.h"
#include "ota_glue.h"
#include "mqtt_glue.h"
#include "io_glue.h"

// Loop pacing: millis() of the last serviced loop iteration.
unsigned long startTime = 0UL;

void setup()
{
    // Pins first so the pumps are driven off before anything can block.
    // Nothing may precede this call: until pinMode()/digitalWrite() run, the
    // MOSFET gates are undriven and the pumps depend entirely on the 10k gate
    // pulldowns (R2/R4) to stay off. Every millisecond spent before this is
    // time both pumps can be energised on a board where those are missing.
    ioSetup();

    // Was delay(200) before ioSetup() + delay(800) after; merged, since the
    // total settling time ahead of Serial.begin() is unchanged.
    delay(1000);
    Serial.begin(115200);

    startTime = millis();

    delay(1000);

    Serial.println("Hello World Giessanlage!");

    initSecrets();
    wifiSetup();
    otaSetup();
    mqttSetup();
}

void loop()
{
    unsigned long currentTime = millis();
    unsigned long elapsedTime = currentTime - startTime;

    if (elapsedTime < 100)
    {
        delay(50);
        return;
    }

    startTime = currentTime;

    anlage.tick(elapsedTime);
    wifiTick(elapsedTime);
    otaTick();
    mqttTick(currentTime);
    ioTick(currentTime, elapsedTime);
}
