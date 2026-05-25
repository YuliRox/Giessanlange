#include "Secrets.h"

namespace
{
constexpr const char *KEY_WIFI_SSID   = "wifi_ssid";
constexpr const char *KEY_WIFI_PASS   = "wifi_pass";
constexpr const char *KEY_MQTT_USER   = "mqtt_user";
constexpr const char *KEY_MQTT_PASS   = "mqtt_pass";
constexpr const char *KEY_MQTT_BROKER = "mqtt_broker";
} // namespace

Secrets::Secrets(KvStore store, const BuildTimeValues &buildTime)
    : _store(std::move(store))
{
    seed(KEY_WIFI_SSID, buildTime.wifiSsid);
    seed(KEY_WIFI_PASS, buildTime.wifiPass);
    seed(KEY_MQTT_USER, buildTime.mqttUser);
    seed(KEY_MQTT_PASS, buildTime.mqttPass);
    seed(KEY_MQTT_BROKER, buildTime.mqttBroker);
}

void Secrets::seed(const std::string &key, const std::string &macroValue)
{
    if (macroValue.empty())
        return;
    if (_store.get(key) != macroValue)
        _store.put(key, macroValue);
}

std::string Secrets::wifiSsid() const   { return _store.get(KEY_WIFI_SSID); }
std::string Secrets::wifiPass() const   { return _store.get(KEY_WIFI_PASS); }
std::string Secrets::mqttUser() const   { return _store.get(KEY_MQTT_USER); }
std::string Secrets::mqttPass() const   { return _store.get(KEY_MQTT_PASS); }
std::string Secrets::mqttBroker() const { return _store.get(KEY_MQTT_BROKER); }

bool Secrets::hasCredentials() const
{
    return !wifiSsid().empty();
}
