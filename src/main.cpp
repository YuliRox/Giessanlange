#include <Arduino.h>

#include "app_state.h"
#include "wifi_glue.h"
#include "mqtt_glue.h"
#include "io_glue.h"

// Loop pacing: millis() of the last serviced loop iteration.
unsigned long startTime = 0UL;

void setup()
{
    delay(200);

    // Pins first so the pumps are driven off before anything can block.
    ioSetup();

    delay(800);
    Serial.begin(115200);

    startTime = millis();

    delay(1000);

    Serial.println("Hello World Giessanlage!");

    initSecrets();
    wifiSetup();
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
    mqttTick(currentTime);
    ioTick(currentTime, elapsedTime);
}
