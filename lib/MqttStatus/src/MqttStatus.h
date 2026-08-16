#ifndef MqttStatus_h
#define MqttStatus_h

#include <functional>
#include <string>

/// Builds the retained status snapshot published to `giessanlage/status`.
/// Platform-independent: hand-rolled JSON, no Arduino headers, all
/// transport via an injected publish callable. Coalesces rapid state
/// changes to at most one publish per `minIntervalMs` so a chain of
/// state transitions doesn't spam the broker.
class MqttStatus
{
public:
    struct Snapshot
    {
        int stateCh1 = 0;
        int stateCh2 = 0;
        unsigned long remainingPumpMsCh1 = 0;
        unsigned long remainingPumpMsCh2 = 0;
        unsigned long remainingWateringMs = 0;
        unsigned long pumpTimeCh1Ms = 0;
        unsigned long pumpTimeCh2Ms = 0;
        unsigned long wateringIntervalMs = 0;
        unsigned long uptimeMs = 0;
        // True when no channel is pumping. Not part of equality/payload; it
        // only selects which throttle (minIntervalMs vs idleIntervalMs)
        // applies, since remainingWateringMs otherwise looks "changed" on
        // essentially every tick while idle. The wide idle floor applies to
        // that countdown churn only — a change in stateCh1/stateCh2 always
        // publishes at minIntervalMs, so pump on/off edges are never delayed.
        bool allIdle = false;

        bool operator==(const Snapshot &other) const;
        bool operator!=(const Snapshot &other) const { return !(*this == other); }
    };

    /// Returns true on a successful publish, false if the transport
    /// rejected (e.g. broker offline). The status logic uses that to
    /// decide whether to mark the snapshot as published.
    using PublishFn = std::function<bool(const std::string &topic,
                                         const std::string &payload,
                                         bool retained)>;

    struct Config
    {
        std::string topic = "giessanlage/status";
        // Min gap between publishes triggered by a meaningful state change
        // (coalesces bursts of transitions) while a channel is pumping.
        unsigned long minIntervalMs = 1000UL;
        // Same, but while all channels are idle. remainingWateringMs counts
        // down continuously, so without a wider floor here idle status would
        // publish at minIntervalMs cadence forever with nothing actually
        // noteworthy to report.
        unsigned long idleIntervalMs = 5UL * 60UL * 1000UL;
        // Republish cadence when nothing meaningful changed (heartbeat), so
        // the retained snapshot stays fresh without publishing every tick.
        unsigned long heartbeatIntervalMs = 30UL * 1000UL;
    };

    MqttStatus(PublishFn publish, Config config);

    /// Drive from the main loop. Publishes when a meaningful field changed
    /// (subject to `minIntervalMs` throttling) or, if nothing changed, once
    /// `heartbeatIntervalMs` has elapsed since the last publish. `uptimeMs`
    /// is not a meaningful field — it only rides along on heartbeats. Returns
    /// true iff a publish was issued this call.
    bool update(const Snapshot &snapshot, unsigned long nowMs);

    /// Force a republish on the next `update()` call. Use after MQTT
    /// reconnects so the broker sees current truth even if the snapshot
    /// hasn't changed.
    void invalidate();

    /// Build the JSON payload for a snapshot. Exposed for testing.
    static std::string buildPayload(const Snapshot &snapshot);

private:
    PublishFn _publish;
    Config _config;

    bool _hasPublished = false;
    Snapshot _lastPublished;
    unsigned long _lastPublishedMs = 0;
};

#endif
