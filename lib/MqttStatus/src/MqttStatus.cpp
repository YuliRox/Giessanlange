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
           wateringIntervalMs == other.wateringIntervalMs &&
           uptimeMs == other.uptimeMs;
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
    // No-op if the snapshot is identical to the last successfully
    // published one. Invalidation (post-reconnect) clears _hasPublished
    // so the next call always publishes.
    if (_hasPublished && snapshot == _lastPublished)
        return false;

    // Throttle: don't publish more than once per minIntervalMs.
    if (_hasPublished && (nowMs - _lastPublishedMs) < _config.minIntervalMs)
        return false;

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
