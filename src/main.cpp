#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <map>
#include <string>

#include "Giessanlage.h"
#include "DebouncedButton.h"
#include "Secrets.h"
#include "WifiManager.h"

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

// NVS namespace shared by Secrets and any future Preferences-backed config.
constexpr const char *PREFS_NAMESPACE = "giessanlage";
constexpr int BUTTON_PUMP_1 = 11;
constexpr int BUTTON_PUMP_2 = 10;
constexpr int BUTTON_CANCEL = 1;
constexpr int PUMP_1_GPIO = 23;
constexpr int PUMP_2_GPIO = 22;
} // namespace

DebouncedButton buttonPump1([] { return digitalRead(BUTTON_PUMP_1); });
DebouncedButton buttonPump2([] { return digitalRead(BUTTON_PUMP_2); });
DebouncedButton buttonCancel([] { return digitalRead(BUTTON_CANCEL); });

Preferences prefs;

// RAM-only fallback store used when NVS init fails. Lets the device run
// on build-time credentials even with a corrupted Preferences partition;
// see setup() for the fallback wiring and the rationale logged at boot.
static std::map<std::string, std::string> ramFallbackStore;

// Lazily initialised after Preferences.begin() succeeds (or its fallback
// kicks in).
Secrets *secrets = nullptr;
WifiManager *wifi = nullptr;

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

static const char *wifiStateName(WifiManager::State s)
{
    switch (s)
    {
    case WifiManager::State::Disconnected: return "Disconnected";
    case WifiManager::State::Connecting:   return "Connecting";
    case WifiManager::State::Connected:    return "Connected";
    }
    return "?";
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

    // Preferences.begin() can fail when the NVS partition is corrupted,
    // the namespace is invalid, or flash is out of free entries. The
    // failure is silent at the Arduino-API level — every subsequent
    // get/put returns "" / false — which would make the device look
    // like it had no credentials at all even when secrets.ini was
    // populated. Surface the failure on serial AND fall back to a
    // RAM-only KvStore so build-time credentials still drive WiFi
    // association this boot. Persistence is lost across reboots, but
    // the device remains reachable for diagnosis instead of going
    // silently offline.
    const bool nvsOk = prefs.begin(PREFS_NAMESPACE, /*readOnly=*/false);
    if (!nvsOk)
    {
        Serial.println("ERROR: NVS init failed — using build-time "
                       "credentials without persistence. Runtime "
                       "credential updates will be lost on reboot.");
    }

    Secrets::KvStore store = nvsOk
        ? Secrets::KvStore{
              [](const std::string &key) {
                  return std::string(prefs.getString(key.c_str(), "").c_str());
              },
              [](const std::string &key, const std::string &value) {
                  prefs.putString(key.c_str(), value.c_str());
              },
          }
        : Secrets::KvStore{
              [](const std::string &key) {
                  auto it = ramFallbackStore.find(key);
                  return it == ramFallbackStore.end() ? std::string() : it->second;
              },
              [](const std::string &key, const std::string &value) {
                  ramFallbackStore[key] = value;
              },
          };

    Secrets::BuildTimeValues build{
        WIFI_SSID,
        WIFI_PASS,
        MQTT_USER,
        MQTT_PASS,
        MQTT_BROKER,
    };

    secrets = new Secrets(std::move(store), build);

    // Configure the radio once for home-network station use. Persistent
    // credential storage is disabled because Secrets / NVS owns that;
    // letting the WiFi class double-write its own copy on every begin()
    // would just burn flash. Auto-reconnect lets the framework re-
    // associate after a transient AP drop without us re-issuing begin();
    // the WifiManager's Connected->Disconnected transition then only
    // fires on prolonged outages.
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
        secrets->wifiSsid(),
        secrets->wifiPass());

    if (!secrets->hasCredentials())
        Serial.println("WiFi: no credentials configured — staying offline");
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

    if (wifi != nullptr && wifi->tick(elapsedTime))
    {
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
