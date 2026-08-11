#include "ota_glue.h"

#include <Arduino.h>
#include <ArduinoOTA.h>

#include "Secrets.h"
#include "app_state.h"

namespace
{
bool otaEnabled = false;
} // namespace

void otaSetup()
{
    if (secrets->otaPass().empty())
    {
        Serial.println("OTA: no password configured — OTA listener disabled");
        return;
    }

    // Matches WiFi.setHostname("giessanlage") in wifi_glue.cpp, so the device
    // is reachable as giessanlage.local for both mDNS and OTA upload.
    ArduinoOTA.setHostname("giessanlage");
    ArduinoOTA.setPassword(secrets->otaPass().c_str());

    ArduinoOTA.onStart([]() { Serial.println("OTA: update starting"); });
    ArduinoOTA.onEnd([]() { Serial.println("OTA: update complete"); });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.print("OTA: error, code=");
        Serial.println(error);
    });

    ArduinoOTA.begin();
    otaEnabled = true;
    Serial.println("OTA: listener started");
}

void otaTick()
{
    if (!otaEnabled)
        return;
    ArduinoOTA.handle();
}
