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
        unsigned long minIntervalMs = 1000;
    };

    MqttStatus(PublishFn publish, Config config);

    /// Drive from the main loop. Publishes when the snapshot differs
    /// from the last successfully published one AND at least
    /// `minIntervalMs` has elapsed since that publish. Returns true iff
    /// a publish was issued this call.
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
