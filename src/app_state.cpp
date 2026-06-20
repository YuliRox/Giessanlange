#include "app_state.h"

#include <Arduino.h>
#include <Preferences.h>
#include <map>
#include <string>

#include "Secrets.h"

Giessanlage anlage;
Secrets    *secrets = nullptr;

namespace
{
// Secrets live in their own NVS namespace, separate from per-feature
// config, so a Secrets sync cannot clobber config keys.
constexpr const char *PREFS_SECRETS = "giessanlage";

Preferences secretsPrefs;

// RAM-only fallback store used when NVS init fails. Lets the device run on
// build-time credentials even with a corrupted Preferences partition;
// persistence is lost across reboots, but the device stays reachable for
// diagnosis instead of going silently offline.
std::map<std::string, std::string> secretsFallbackStore;
} // namespace

void initSecrets()
{
    // Preferences.begin() can fail when the NVS partition is corrupted, the
    // namespace is invalid, or flash is out of free entries. The failure is
    // silent at the Arduino-API level — every subsequent get/put returns
    // "" / false — which would make the device look like it had no
    // credentials at all even when secrets.ini was populated. Surface it on
    // serial AND fall back to a RAM-only KvStore.
    const bool secretsNvsOk = secretsPrefs.begin(PREFS_SECRETS, /*readOnly=*/false);
    if (!secretsNvsOk)
    {
        Serial.println("ERROR: Secrets NVS init failed — using build-time "
                       "credentials without persistence. Runtime credential "
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
}
