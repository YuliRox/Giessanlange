#include "MqttEvents.h"
#include <cstdio>

namespace
{
const char *stateName(int state)
{
    switch (state)
    {
    case 1: return "Idle";
    case 2: return "PumpingManual";
    case 3: return "PumpingAuto";
    default: return "Undefined";
    }
}
} // namespace

MqttEvents::MqttEvents(PublishFn publish, Config config)
    : _publish(std::move(publish)), _config(std::move(config))
{
}

std::string MqttEvents::buildPumpPayload(int channel, int fromState, int toState,
                                         unsigned long tsMs)
{
    char buf[160];
    std::snprintf(buf, sizeof(buf),
        "{\"channel\":%d,\"from\":\"%s\",\"to\":\"%s\",\"ts_ms\":%lu}",
        channel, stateName(fromState), stateName(toState), tsMs);
    return std::string(buf);
}

std::string MqttEvents::buildButtonPayload(const std::string &button, unsigned long tsMs)
{
    // Sanitise: reject any string that could break the JSON structure.
    const bool safe = button.find('"') == std::string::npos &&
                      button.find('\\') == std::string::npos;
    const char *label = safe ? button.c_str() : "unknown";
    char buf[96];
    std::snprintf(buf, sizeof(buf),
        "{\"button\":\"%s\",\"ts_ms\":%lu}",
        label, tsMs);
    return std::string(buf);
}

int MqttEvents::updatePumpStates(const ChannelState &current, unsigned long nowMs)
{
    if (!_initialized)
    {
        _last = current;
        _initialized = true;
        return 0;
    }

    int count = 0;
    if (current.stateCh1 != _last.stateCh1)
    {
        const std::string payload =
            buildPumpPayload(1, _last.stateCh1, current.stateCh1, nowMs);
        if (_publish(_config.pumpTopic, payload, /*retained=*/false))
            ++count;
    }
    if (current.stateCh2 != _last.stateCh2)
    {
        const std::string payload =
            buildPumpPayload(2, _last.stateCh2, current.stateCh2, nowMs);
        if (_publish(_config.pumpTopic, payload, /*retained=*/false))
            ++count;
    }
    _last = current;
    return count;
}

bool MqttEvents::publishButton(const std::string &button, unsigned long nowMs)
{
    return _publish(_config.buttonTopic, buildButtonPayload(button, nowMs),
                    /*retained=*/false);
}

void MqttEvents::resetBaseline()
{
    _initialized = false;
}
