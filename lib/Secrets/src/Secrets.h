#ifndef Secrets_h
#define Secrets_h

#include <functional>
#include <string>

/// Persistent secrets (WiFi + MQTT credentials) backed by an injected
/// key-value store. On every boot, reconciles the store against the
/// build-time macro values: a non-empty macro that differs from what the
/// store holds is treated as authoritative and overwrites it. Platform-
/// independent so it can be unit-tested on the host; the Arduino side
/// wraps the ESP32 Preferences API with a KvStore.
class Secrets
{
public:
    struct KvStore
    {
        std::function<std::string(const std::string &)> get;
        std::function<void(const std::string &, const std::string &)> put;
    };

    struct BuildTimeValues
    {
        std::string wifiSsid;
        std::string wifiPass;
        std::string mqttUser;
        std::string mqttPass;
        std::string mqttBroker;
        std::string otaPass;
    };

    /// Construct, syncing the store from any non-empty build-time value
    /// that differs from what the store already holds. Empty build-time
    /// values are ignored — the store keeps whatever it had.
    Secrets(KvStore store, const BuildTimeValues &buildTime);

    std::string wifiSsid() const;
    std::string wifiPass() const;
    std::string mqttUser() const;
    std::string mqttPass() const;
    std::string mqttBroker() const;
    std::string otaPass() const;

    /// True iff `wifiSsid()` is non-empty. Use as a quick gate before
    /// attempting WiFi association on devices with no credentials yet.
    bool hasCredentials() const;

private:
    KvStore _store;

    void syncFromBuildTime(const std::string &key, const std::string &macroValue);
};

#endif
