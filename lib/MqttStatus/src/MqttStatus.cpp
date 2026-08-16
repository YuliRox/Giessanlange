#include "MqttStatus.h"
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

bool MqttStatus::Snapshot::operator==(const Snapshot &other) const
{
    return stateCh1 == other.stateCh1 &&
           stateCh2 == other.stateCh2 &&
           remainingPumpMsCh1 == other.remainingPumpMsCh1 &&
           remainingPumpMsCh2 == other.remainingPumpMsCh2 &&
           remainingWateringMs == other.remainingWateringMs &&
           pumpTimeCh1Ms == other.pumpTimeCh1Ms &&
           pumpTimeCh2Ms == other.pumpTimeCh2Ms &&
           wateringIntervalMs == other.wateringIntervalMs;
    // uptimeMs intentionally excluded: it changes every tick. Periodic
    // republishing is driven by Config::heartbeatIntervalMs in update(),
    // not by treating every uptime increment as a "change".
}

MqttStatus::MqttStatus(PublishFn publish, Config config)
    : _publish(std::move(publish)), _config(std::move(config))
{
}

std::string MqttStatus::buildPayload(const Snapshot &s)
{
    char buf[512];
    std::snprintf(buf, sizeof(buf),
        "{"
        "\"state_ch1\":\"%s\","
        "\"state_ch2\":\"%s\","
        "\"remaining_pump_ms_ch1\":%lu,"
        "\"remaining_pump_ms_ch2\":%lu,"
        "\"remaining_watering_ms\":%lu,"
        "\"pump_time_ch1_ms\":%lu,"
        "\"pump_time_ch2_ms\":%lu,"
        "\"watering_interval_ms\":%lu,"
        "\"uptime_ms\":%lu"
        "}",
        stateName(s.stateCh1),
        stateName(s.stateCh2),
        s.remainingPumpMsCh1,
        s.remainingPumpMsCh2,
        s.remainingWateringMs,
        s.pumpTimeCh1Ms,
        s.pumpTimeCh2Ms,
        s.wateringIntervalMs,
        s.uptimeMs);
    return std::string(buf);
}

bool MqttStatus::update(const Snapshot &snapshot, unsigned long nowMs)
{
    // First publish (or post-reconnect invalidate) always goes out.
    if (_hasPublished)
    {
        const unsigned long sincePublish = nowMs - _lastPublishedMs;
        const bool changed = snapshot != _lastPublished; // uptime excluded
        if (changed)
        {
            // Meaningful change: publish, but throttle bursts. The wide idle
            // floor only applies to countdown churn — remainingWateringMs
            // alone keeps this branch "changed" on nearly every tick while
            // idle. A channel state transition is never churn, so it always
            // uses minIntervalMs; otherwise the edge into Idle (the one that
            // says "pump finished") would be held back for idleIntervalMs.
            const bool stateChanged = snapshot.stateCh1 != _lastPublished.stateCh1 ||
                                      snapshot.stateCh2 != _lastPublished.stateCh2;
            const unsigned long floorMs =
                (snapshot.allIdle && !stateChanged) ? _config.idleIntervalMs
                                                    : _config.minIntervalMs;
            if (sincePublish < floorMs)
                return false;
        }
        else
        {
            // Nothing meaningful changed: only the periodic heartbeat
            // republishes, so we don't spam the broker every tick.
            if (sincePublish < _config.heartbeatIntervalMs)
                return false;
        }
    }

    const std::string payload = buildPayload(snapshot);
    if (!_publish(_config.topic, payload, /*retained=*/true))
        return false;

    _hasPublished = true;
    _lastPublished = snapshot;
    _lastPublishedMs = nowMs;
    return true;
}

void MqttStatus::invalidate()
{
    _hasPublished = false;
}
