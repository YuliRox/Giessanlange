#include "MqttConfig.h"
#include <cstdio>
#include <cstdlib>

namespace
{
// JSON field names on the giessanlage/config topic (broker-facing schema).
constexpr const char *KEY_CH1 = "pump_time_ch1_ms";
constexpr const char *KEY_CH2 = "pump_time_ch2_ms";
constexpr const char *KEY_INT = "watering_interval_ms";
constexpr const char *KEY_PAUSE = "paused";

// NVS keys. ESP32 NVS keys are capped at 15 chars, so these are short
// aliases of the JSON names above (which exceed the limit). Do not lengthen.
constexpr const char *NVS_CH1 = "pump_t_ch1_ms";
constexpr const char *NVS_CH2 = "pump_t_ch2_ms";
constexpr const char *NVS_INT = "water_int_ms";
constexpr const char *NVS_PAUSE = "paused";

bool parseUlongField(const std::string &json, const std::string &key,
                     unsigned long &out)
{
    const std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos)
        return false;
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos)
        return false;
    const char *start = json.c_str() + pos + 1;
    char *end = nullptr;
    unsigned long v = std::strtoul(start, &end, 10);
    if (end == start)
        return false;
    out = v;
    return true;
}

// Accepts the JSON literals true/false as well as 1/0, since consumers differ:
// Home Assistant templates emit bare booleans, while hand-rolled publishers and
// shell one-liners tend to emit integers.
bool parseBoolField(const std::string &json, const std::string &key, bool &out)
{
    const std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos)
        return false;
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos)
        return false;

    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' ||
                                 json[pos] == '\n' || json[pos] == '\r'))
        ++pos;
    if (pos >= json.size())
        return false;

    if (json.compare(pos, 4, "true") == 0) { out = true;  return true; }
    if (json.compare(pos, 5, "false") == 0) { out = false; return true; }
    if (json[pos] == '1') { out = true;  return true; }
    if (json[pos] == '0') { out = false; return true; }
    return false;
}

unsigned long readUlongOrDefault(const MqttConfig::KvStore &store,
                                 const std::string &key,
                                 unsigned long fallback)
{
    const std::string raw = store.get(key);
    if (raw.empty())
        return fallback;
    char *end = nullptr;
    unsigned long v = std::strtoul(raw.c_str(), &end, 10);
    if (end == raw.c_str())
        return fallback;
    return v;
}

bool readBoolOrDefault(const MqttConfig::KvStore &store, const std::string &key,
                       bool fallback)
{
    const std::string raw = store.get(key);
    if (raw.empty())
        return fallback;
    return raw == "1" || raw == "true";
}

std::string toString(unsigned long v)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%lu", v);
    return std::string(buf);
}
} // namespace

MqttConfig::MqttConfig(KvStore store, PublishFn publish, ApplyFn apply,
                       Config config)
    : _store(std::move(store)),
      _publish(std::move(publish)),
      _apply(std::move(apply)),
      _config(std::move(config))
{
}

std::string MqttConfig::buildPayload(const Values &v)
{
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "{\"%s\":%lu,\"%s\":%lu,\"%s\":%lu,\"%s\":%s}",
        KEY_CH1, v.pumpTimeCh1Ms,
        KEY_CH2, v.pumpTimeCh2Ms,
        KEY_INT, v.wateringIntervalMs,
        KEY_PAUSE, v.paused ? "true" : "false");
    return std::string(buf);
}

bool MqttConfig::parsePayload(const std::string &json, Values &out)
{
    Values v;
    if (!parseUlongField(json, KEY_CH1, v.pumpTimeCh1Ms)) return false;
    if (!parseUlongField(json, KEY_CH2, v.pumpTimeCh2Ms)) return false;
    if (!parseUlongField(json, KEY_INT, v.wateringIntervalMs)) return false;

    // Optional on purpose. A retained config published by an older firmware
    // has no "paused" key, and rejecting it would strand the device on
    // defaults after an upgrade. Absent means not paused, which is also the
    // only safe default: a parse quirk must never silently stop watering.
    if (!parseBoolField(json, KEY_PAUSE, v.paused))
        v.paused = false;

    out = v;
    return true;
}

bool MqttConfig::isValid(const Values &v) const
{
    if (v.pumpTimeCh1Ms == 0) return false;
    if (v.pumpTimeCh2Ms == 0) return false;
    if (v.wateringIntervalMs == 0) return false;
    if (v.pumpTimeCh1Ms >= v.wateringIntervalMs) return false;
    if (v.pumpTimeCh2Ms >= v.wateringIntervalMs) return false;
    return true;
}

MqttConfig::Values MqttConfig::initFromNvs()
{
    // Require all three keys: if only some survived NVS corruption, treat
    // it as unseeded and re-persist the full set below, rather than booting
    // with a hybrid (some-stored / some-default) config that never repairs.
    const bool nvsHasAllKeys = !_store.get(NVS_CH1).empty() &&
                               !_store.get(NVS_CH2).empty() &&
                               !_store.get(NVS_INT).empty();

    _current.pumpTimeCh1Ms     = readUlongOrDefault(_store, NVS_CH1, _config.defaults.pumpTimeCh1Ms);
    _current.pumpTimeCh2Ms     = readUlongOrDefault(_store, NVS_CH2, _config.defaults.pumpTimeCh2Ms);
    _current.wateringIntervalMs = readUlongOrDefault(_store, NVS_INT, _config.defaults.wateringIntervalMs);

    // Deliberately not part of nvsHasAllKeys above: an NVS store written by an
    // older firmware has the three numeric keys but no pause key, and treating
    // that as unseeded would rewrite a perfectly good config. Absent reads as
    // not paused, and persist() adds the key on the next config change.
    _current.paused = readBoolOrDefault(_store, NVS_PAUSE, _config.defaults.paused);

    // Seed NVS on first boot (or repair partial corruption) so subsequent
    // boots are no-ops if defaults haven't changed.
    if (!nvsHasAllKeys)
        persist(_current);

    _apply(_current);
    return _current;
}

void MqttConfig::onMqttConnected()
{
    _brokerSeenThisSession = false;
}

bool MqttConfig::onConfigPayload(const std::string &jsonPayload)
{
    Values incoming;
    if (!parsePayload(jsonPayload, incoming))
        return false;
    if (!isValid(incoming))
        return false;

    _brokerSeenThisSession = true;

    if (incoming == _current)
        return false; // matches NVS; nothing to do

    _current = incoming;
    persist(_current);
    _apply(_current);
    return true;
}

bool MqttConfig::publishIfBrokerSilent()
{
    if (_brokerSeenThisSession)
        return false;
    if (!_publish(_config.topic, buildPayload(_current), /*retained=*/true))
        return false;
    _brokerSeenThisSession = true;
    return true;
}

bool MqttConfig::brokerHasSpoken() const
{
    return _brokerSeenThisSession;
}

MqttConfig::Values MqttConfig::currentValues() const
{
    return _current;
}

void MqttConfig::persist(const Values &v)
{
    _store.put(NVS_CH1, toString(v.pumpTimeCh1Ms));
    _store.put(NVS_CH2, toString(v.pumpTimeCh2Ms));
    _store.put(NVS_INT, toString(v.wateringIntervalMs));
    _store.put(NVS_PAUSE, v.paused ? "1" : "0");
}
