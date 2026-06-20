#include "mqtt_glue.h"

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <cstring>
#include <map>
#include <string>

#include "Giessanlage.h"
#include "Secrets.h"
#include "MqttStatus.h"
#include "MqttEvents.h"
#include "MqttConfig.h"
#include "WifiManager.h"
#include "app_state.h"
#include "wifi_glue.h"

MqttEvents *mqttEvents = nullptr;

namespace
{
// Per-feature config lives in its own NVS namespace, separate from the
// Secrets namespace, so a Secrets sync cannot clobber config keys.
constexpr const char *PREFS_CONFIG = "giessanlage_cfg";

// MQTT broker connection.
constexpr const char *MQTT_CLIENT_ID            = "giessanlage";
constexpr const char *MQTT_AVAILABILITY         = "giessanlage/availability";
constexpr const char *MQTT_CONFIG_TOPIC         = "giessanlage/config";
constexpr int           MQTT_PORT               = 1883;
constexpr unsigned long MQTT_RECONNECT_DELAY_MS = 5000;
constexpr unsigned long MQTT_BROKER_GRACE_MS    = 3000;

Preferences configPrefs;

// RAM-only fallback when config NVS init fails; see initSecrets() for the
// same pattern and rationale on the Secrets side.
std::map<std::string, std::string> configFallbackStore;

WiFiClient   wifiClient;
PubSubClient mqttClient(wifiClient);

// PubSubClient::setServer stores the char* we hand it, not a copy. Keep the
// broker host string alive for the program's lifetime so that pointer stays
// valid.
std::string mqttBrokerHost;

MqttStatus *mqttStatus = nullptr;
MqttConfig *mqttConfig = nullptr;

unsigned long lastMqttConnectAttemptMs = 0;
unsigned long mqttSubscribedAtMs       = 0;
bool          mqttGraceFired           = false;
bool          mqttWasConnected         = false;

bool mqttPublishLambda(const std::string &topic, const std::string &payload, bool retained)
{
    if (!mqttClient.connected())
        return false;
    return mqttClient.publish(topic.c_str(), payload.c_str(), retained);
}

void onMqttMessage(char *topic, byte *payload, unsigned int length)
{
    std::string p(reinterpret_cast<char *>(payload), length);
    if (std::strcmp(topic, MQTT_CONFIG_TOPIC) == 0 && mqttConfig != nullptr)
    {
        const bool changed = mqttConfig->onConfigPayload(p);
        if (changed && mqttStatus != nullptr)
            mqttStatus->invalidate();
    }
}

bool tryMqttConnect(unsigned long nowMs)
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
} // namespace

void mqttSetup()
{
    const bool configNvsOk = configPrefs.begin(PREFS_CONFIG, /*readOnly=*/false);
    if (!configNvsOk)
    {
        Serial.println("ERROR: Config NVS init failed — using firmware "
                       "defaults without persistence. Broker-driven config "
                       "updates will be lost on reboot.");
    }

    // Config is wired first because initFromNvs() applies to Giessanlage
    // immediately, before WiFi/MQTT come up.
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
    cfgCfg.defaults.pumpTimeCh1Ms      = Giessanlage::INTERVAL_30S;
    cfgCfg.defaults.pumpTimeCh2Ms      = Giessanlage::INTERVAL_30S;
    cfgCfg.defaults.wateringIntervalMs = Giessanlage::INTERVAL_24H;

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

void mqttTick(unsigned long nowMs)
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
