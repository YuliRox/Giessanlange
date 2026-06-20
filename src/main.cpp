#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <cstring>
#include <map>
#include <string>

#include "Giessanlage.h"
#include "DebouncedButton.h"
#include "Secrets.h"
#include "WifiManager.h"
#include "MqttStatus.h"
#include "MqttEvents.h"
#include "MqttConfig.h"

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

// Pin map per docs/GPIO_MAPPING.md + the schematic (ESP32-C6 rework).
constexpr int BUTTON_PUMP_1 = 11;
constexpr int BUTTON_PUMP_2 = 10;
constexpr int BUTTON_CANCEL = 1;
constexpr int PUMP_1_GPIO = 23;
constexpr int PUMP_2_GPIO = 22;

// NVS namespaces. Secrets and per-feature config live in separate
// namespaces so a Secrets sync cannot clobber config keys.
constexpr const char *PREFS_SECRETS = "giessanlage";
constexpr const char *PREFS_CONFIG  = "giessanlage_cfg";

// MQTT broker connection.
constexpr const char *MQTT_CLIENT_ID         = "giessanlage";
constexpr const char *MQTT_AVAILABILITY      = "giessanlage/availability";
constexpr const char *MQTT_CONFIG_TOPIC      = "giessanlage/config";
constexpr int         MQTT_PORT              = 1883;
constexpr unsigned long MQTT_RECONNECT_DELAY_MS = 5000;
constexpr unsigned long MQTT_BROKER_GRACE_MS    = 3000;
} // namespace

DebouncedButton buttonPump1([] { return digitalRead(BUTTON_PUMP_1); });
DebouncedButton buttonPump2([] { return digitalRead(BUTTON_PUMP_2); });
DebouncedButton buttonCancel([] { return digitalRead(BUTTON_CANCEL); });

Preferences secretsPrefs;
Preferences configPrefs;

WiFiClient   wifiClient;
PubSubClient mqttClient(wifiClient);

// RAM-only fallback stores used when NVS init fails. Lets the device
// run on build-time credentials (Secrets) and firmware-default config
// (MqttConfig) even with a corrupted Preferences partition; see
// setup() for the fallback wiring and the rationale logged at boot.
static std::map<std::string, std::string> secretsFallbackStore;
static std::map<std::string, std::string> configFallbackStore;

// Lazily initialised after Preferences.begin() succeeds (or its fallback
// kicks in).
// PubSubClient::setServer stores the char* we hand it, not a copy. Keep the
// broker host string alive for the program's lifetime so that pointer stays valid.
static std::string mqttBrokerHost;

Secrets     *secrets    = nullptr;
WifiManager *wifi       = nullptr;
MqttStatus  *mqttStatus = nullptr;
MqttEvents  *mqttEvents = nullptr;
MqttConfig  *mqttConfig = nullptr;

const unsigned long outputRemainingWaitInterval = 60UL * 1000UL;
unsigned long outputRemainingWait = 0;

unsigned long lastMqttConnectAttemptMs = 0;
unsigned long mqttSubscribedAtMs = 0;
bool          mqttGraceFired = false;
bool          mqttWasConnected = false;

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

static bool mqttPublishLambda(const std::string &topic, const std::string &payload,
                              bool retained)
{
    if (!mqttClient.connected())
        return false;
    return mqttClient.publish(topic.c_str(), payload.c_str(), retained);
}

static void onMqttMessage(char *topic, byte *payload, unsigned int length)
{
    std::string p(reinterpret_cast<char *>(payload), length);
    if (std::strcmp(topic, MQTT_CONFIG_TOPIC) == 0 && mqttConfig != nullptr)
    {
        const bool changed = mqttConfig->onConfigPayload(p);
        if (changed && mqttStatus != nullptr)
            mqttStatus->invalidate();
    }
}

static bool tryMqttConnect(unsigned long nowMs)
{
    if (mqttClient.connected())
        return true;
    if (wifi == nullptr || !wifi->isConnected())
        return false;
    if (secrets == nullptr || secrets->mqttBroker().empty())
        return false;
    if (nowMs - lastMqttConnectAttemptMs < MQTT_RECONNECT_DELAY_MS &&
        lastMqttConnectAttemptMs != 0)
        return false;
    lastMqttConnectAttemptMs = nowMs;

    Serial.print("MQTT: connecting to ");
    Serial.println(secrets->mqttBroker().c_str());

    const bool ok = mqttClient.connect(
        MQTT_CLIENT_ID,
        secrets->mqttUser().c_str(),
        secrets->mqttPass().c_str(),
        MQTT_AVAILABILITY,
        /*willQoS=*/0, /*willRetain=*/true, "offline");
    if (!ok)
    {
        Serial.print("MQTT: connect failed, state=");
        Serial.println(mqttClient.state());
        return false;
    }

    mqttClient.publish(MQTT_AVAILABILITY, "online", /*retained=*/true);
    mqttClient.subscribe(MQTT_CONFIG_TOPIC);
    mqttSubscribedAtMs = nowMs;
    mqttGraceFired = false;

    if (mqttConfig != nullptr) mqttConfig->onMqttConnected();
    if (mqttEvents != nullptr) mqttEvents->resetBaseline();
    if (mqttStatus != nullptr) mqttStatus->invalidate();

    Serial.println("MQTT: connected");
    return true;
}

static void tickMqtt(unsigned long nowMs)
{
    if (!tryMqttConnect(nowMs))
    {
        if (mqttWasConnected)
        {
            Serial.println("MQTT: disconnected");
            mqttWasConnected = false;
        }
        return;
    }
    mqttWasConnected = true;
    mqttClient.loop();

    if (mqttStatus != nullptr)
    {
        MqttStatus::Snapshot s;
        s.stateCh1            = static_cast<int>(anlage.getState(Channel::One));
        s.stateCh2            = static_cast<int>(anlage.getState(Channel::Two));
        s.remainingPumpMsCh1  = anlage.getRemainingPumpTime(Channel::One);
        s.remainingPumpMsCh2  = anlage.getRemainingPumpTime(Channel::Two);
        s.remainingWateringMs = anlage.getRemainingWateringInterval();
        s.pumpTimeCh1Ms       = anlage.getPumpTime(Channel::One);
        s.pumpTimeCh2Ms       = anlage.getPumpTime(Channel::Two);
        s.wateringIntervalMs  = anlage.getWateringInterval();
        s.uptimeMs            = nowMs;
        mqttStatus->update(s, nowMs);
    }

    if (mqttEvents != nullptr)
    {
        MqttEvents::ChannelState st;
        st.stateCh1 = static_cast<int>(anlage.getState(Channel::One));
        st.stateCh2 = static_cast<int>(anlage.getState(Channel::Two));
        mqttEvents->updatePumpStates(st, nowMs);
    }

    if (mqttConfig != nullptr && !mqttGraceFired &&
        (nowMs - mqttSubscribedAtMs) >= MQTT_BROKER_GRACE_MS)
    {
        if (mqttConfig->brokerHasSpoken())
            mqttGraceFired = true;
        else if (mqttConfig->publishIfBrokerSilent())
            mqttGraceFired = true;
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

    // Preferences.begin() can fail when the NVS partition is corrupted,
    // the namespace is invalid, or flash is out of free entries. The
    // failure is silent at the Arduino-API level — every subsequent
    // get/put returns "" / false — which would make the device look
    // like it had no credentials at all even when secrets.ini was
    // populated. Surface the failure on serial AND fall back to a
    // RAM-only KvStore per namespace so build-time credentials still
    // drive WiFi association and firmware defaults still drive watering
    // config this boot. Persistence is lost across reboots, but the
    // device remains reachable for diagnosis instead of going silently
    // offline.
    const bool secretsNvsOk = secretsPrefs.begin(PREFS_SECRETS, /*readOnly=*/false);
    const bool configNvsOk  = configPrefs.begin(PREFS_CONFIG,   /*readOnly=*/false);
    if (!secretsNvsOk)
    {
        Serial.println("ERROR: Secrets NVS init failed — using build-time "
                       "credentials without persistence. Runtime credential "
                       "updates will be lost on reboot.");
    }
    if (!configNvsOk)
    {
        Serial.println("ERROR: Config NVS init failed — using firmware "
                       "defaults without persistence. Broker-driven config "
                       "updates will be lost on reboot.");
    }

    Secrets::KvStore secretsStore = secretsNvsOk
        ? Secrets::KvStore{
              [](const std::string &key) {
                  return std::string(secretsPrefs.getString(key.c_str(), "").c_str());
              },
              [](const std::string &key, const std::string &value) {
                  secretsPrefs.putString(key.c_str(), value.c_str());
              },
          }
        : Secrets::KvStore{
              [](const std::string &key) {
                  auto it = secretsFallbackStore.find(key);
                  return it == secretsFallbackStore.end() ? std::string() : it->second;
              },
              [](const std::string &key, const std::string &value) {
                  secretsFallbackStore[key] = value;
              },
          };

    Secrets::BuildTimeValues build{
        WIFI_SSID, WIFI_PASS, MQTT_USER, MQTT_PASS, MQTT_BROKER,
    };

    secrets = new Secrets(std::move(secretsStore), build);

    // Configure the radio once for home-network station use (see notes
    // in the previous commit).
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

    // MQTT setup. Config is wired first because initFromNvs() applies
    // to Giessanlage immediately, before WiFi/MQTT come up.
    MqttConfig::KvStore cfgStore = configNvsOk
        ? MqttConfig::KvStore{
              [](const std::string &key) {
                  return std::string(configPrefs.getString(key.c_str(), "").c_str());
              },
              [](const std::string &key, const std::string &value) {
                  configPrefs.putString(key.c_str(), value.c_str());
              },
          }
        : MqttConfig::KvStore{
              [](const std::string &key) {
                  auto it = configFallbackStore.find(key);
                  return it == configFallbackStore.end() ? std::string() : it->second;
              },
              [](const std::string &key, const std::string &value) {
                  configFallbackStore[key] = value;
              },
          };

    MqttConfig::Config cfgCfg;
    cfgCfg.defaults.pumpTimeCh1Ms       = Giessanlage::INTERVAL_30S;
    cfgCfg.defaults.pumpTimeCh2Ms       = Giessanlage::INTERVAL_30S;
    cfgCfg.defaults.wateringIntervalMs  = Giessanlage::INTERVAL_24H;

    mqttConfig = new MqttConfig(
        std::move(cfgStore),
        mqttPublishLambda,
        [](const MqttConfig::Values &v) {
            anlage.setPumpTime(Channel::One, v.pumpTimeCh1Ms);
            anlage.setPumpTime(Channel::Two, v.pumpTimeCh2Ms);
            anlage.setWateringInterval(v.wateringIntervalMs);
        },
        cfgCfg);
    mqttConfig->initFromNvs();

    mqttStatus = new MqttStatus(mqttPublishLambda, {});
    mqttEvents = new MqttEvents(mqttPublishLambda, {});

    mqttBrokerHost = secrets->mqttBroker();
    if (!mqttBrokerHost.empty())
        mqttClient.setServer(mqttBrokerHost.c_str(), MQTT_PORT);
    mqttClient.setCallback(onMqttMessage);

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

    tickMqtt(currentTime);

    digitalWrite(PUMP_1_GPIO, anlage.isPumping(Channel::One) ? PUMP_ON : PUMP_OFF);
    digitalWrite(PUMP_2_GPIO, anlage.isPumping(Channel::Two) ? PUMP_ON : PUMP_OFF);

    if (buttonPump1.poll(currentTime))
    {
        togglePump(Channel::One);
        if (mqttEvents != nullptr) mqttEvents->publishButton("pump1", currentTime);
    }
    if (buttonPump2.poll(currentTime))
    {
        togglePump(Channel::Two);
        if (mqttEvents != nullptr) mqttEvents->publishButton("pump2", currentTime);
    }
    if (buttonCancel.poll(currentTime))
    {
        if (anlage.stopAllPumps())
            Serial.println("Cancel: all pumps off");
        if (mqttEvents != nullptr) mqttEvents->publishButton("cancel", currentTime);
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
