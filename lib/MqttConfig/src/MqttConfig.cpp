#include "MqttConfig.h"
#include <cstdio>
#include <cstdlib>

namespace
{
constexpr const char *KEY_CH1 = "pump_time_ch1_ms";
constexpr const char *KEY_CH2 = "pump_time_ch2_ms";
constexpr const char *KEY_INT = "watering_interval_ms";

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
    char buf[192];
    std::snprintf(buf, sizeof(buf),
        "{\"%s\":%lu,\"%s\":%lu,\"%s\":%lu}",
        KEY_CH1, v.pumpTimeCh1Ms,
        KEY_CH2, v.pumpTimeCh2Ms,
        KEY_INT, v.wateringIntervalMs);
    return std::string(buf);
}

bool MqttConfig::parsePayload(const std::string &json, Values &out)
{
    Values v;
    if (!parseUlongField(json, KEY_CH1, v.pumpTimeCh1Ms)) return false;
    if (!parseUlongField(json, KEY_CH2, v.pumpTimeCh2Ms)) return false;
    if (!parseUlongField(json, KEY_INT, v.wateringIntervalMs)) return false;
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
    const bool nvsHasAnyKey = !_store.get(KEY_CH1).empty() ||
                              !_store.get(KEY_CH2).empty() ||
                              !_store.get(KEY_INT).empty();

    _current.pumpTimeCh1Ms     = readUlongOrDefault(_store, KEY_CH1, _config.defaults.pumpTimeCh1Ms);
    _current.pumpTimeCh2Ms     = readUlongOrDefault(_store, KEY_CH2, _config.defaults.pumpTimeCh2Ms);
    _current.wateringIntervalMs = readUlongOrDefault(_store, KEY_INT, _config.defaults.wateringIntervalMs);

    // Seed NVS on first boot so subsequent boots are no-ops if defaults
    // haven't changed.
    if (!nvsHasAnyKey)
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
    _store.put(KEY_CH1, toString(v.pumpTimeCh1Ms));
    _store.put(KEY_CH2, toString(v.pumpTimeCh2Ms));
    _store.put(KEY_INT, toString(v.wateringIntervalMs));
}
