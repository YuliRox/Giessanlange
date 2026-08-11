#include "MqttCommands.h"

#include <cstdlib>

namespace
{
constexpr const char *KEY_CMD     = "cmd";
constexpr const char *KEY_TIME_MS = "time_ms";

constexpr const char *CMD_TOGGLE_PUMP = "toggle_pump";
constexpr const char *CMD_SET_TIMER   = "set_timer";
constexpr const char *CMD_RESET_TIMER = "reset_timer";

bool parseStringField(const std::string &json, const std::string &key,
                      std::string &out)
{
    const std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos)
        return false;
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos)
        return false;
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos)
        return false;
    const auto end = json.find('"', pos + 1);
    if (end == std::string::npos)
        return false;
    out = json.substr(pos + 1, end - pos - 1);
    return true;
}

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
} // namespace

bool MqttCommands::dispatch(Giessanlage &g, Giessanlage::Channel channel,
                            const std::string &jsonPayload, unsigned long nowMs)
{
    std::string cmd;
    if (!parseStringField(jsonPayload, KEY_CMD, cmd))
        return false;

    const int slot = idx(channel);
    if (_hasDispatched[slot] && (nowMs - _lastDispatchMs[slot]) < RATE_LIMIT_MS)
        return false;

    bool applied = false;

    if (cmd == CMD_TOGGLE_PUMP)
    {
        applied = g.isPumping(channel) ? g.stopPump(channel) : g.triggerPump(channel);
    }
    else if (cmd == CMD_SET_TIMER)
    {
        unsigned long timeMs = 0;
        if (parseUlongField(jsonPayload, KEY_TIME_MS, timeMs))
            applied = g.setPumpTime(channel, timeMs);
    }
    else if (cmd == CMD_RESET_TIMER)
    {
        unsigned long timeMs = 0;
        if (parseUlongField(jsonPayload, KEY_TIME_MS, timeMs) &&
            g.setPumpTime(channel, timeMs))
            applied = g.resetPumpTimer(channel);
    }

    if (applied)
    {
        _lastDispatchMs[slot] = nowMs;
        _hasDispatched[slot] = true;
    }

    return applied;
}
