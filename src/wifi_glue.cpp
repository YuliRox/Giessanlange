#include "wifi_glue.h"

#include <Arduino.h>
#include <WiFi.h>
#include <string>

#include "WifiManager.h"
#include "Secrets.h"
#include "app_state.h"

WifiManager *wifi = nullptr;

namespace
{
const char *wifiStateName(WifiManager::State s)
{
    switch (s)
    {
    case WifiManager::State::Disconnected: return "Disconnected";
    case WifiManager::State::Connecting:   return "Connecting";
    case WifiManager::State::Connected:    return "Connected";
    }
    return "?";
}
} // namespace

void wifiSetup()
{
    // Configure the radio once for home-network station use:
    // persistent(false) keeps credentials out of the WiFi-stack NVS (Secrets
    // owns them); STA mode + a stable hostname + auto-reconnect give a plain
    // client that rejoins the AP on transient drops without our intervention.
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setHostname("giessanlage");
    WiFi.setAutoReconnect(true);

    wifi = new WifiManager(
        [](const std::string &ssid, const std::string &pass) {
            Serial.print("WiFi: connecting to '");
            Serial.print(ssid.c_str());
            Serial.println("'");
            WiFi.begin(ssid.c_str(), pass.c_str());
        },
        []() { return WiFi.status() == WL_CONNECTED; },
        secrets->wifiSsid(), secrets->wifiPass());

    if (!secrets->hasCredentials())
        Serial.println("WiFi: no credentials configured — staying offline");
}

void wifiTick(unsigned long elapsedMs)
{
    if (wifi == nullptr || !wifi->tick(elapsedMs))
        return;

    const auto s = wifi->state();
    Serial.print("WiFi: ");
    Serial.print(wifiStateName(s));
    if (s == WifiManager::State::Connected)
    {
        Serial.print(" (");
        Serial.print(WiFi.localIP());
        Serial.print(")");
    }
    Serial.println();
}
