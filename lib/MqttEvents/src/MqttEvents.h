#ifndef MqttEvents_h
#define MqttEvents_h

#include <functional>
#include <string>

/// Live event publishing on top of the retained status (see MqttStatus).
/// Pump events fire on per-channel state transitions; button events fire
/// on confirmed debounced presses. Both topics are non-retained — the
/// retained `status` topic carries the post-transition truth, the event
/// stream is for "something just happened" consumers (logs, automations).
///
/// Events are fire-and-forget: if the broker is offline the publish
/// fails and the event is dropped. We deliberately do NOT buffer —
/// reconnect republishes `status`, which is sufficient to convey
/// after-state.
class MqttEvents
{
public:
    using PublishFn = std::function<bool(const std::string &topic,
                                         const std::string &payload,
                                         bool retained)>;

    struct Config
    {
        std::string pumpTopic = "giessanlage/events/pump";
        std::string buttonTopic = "giessanlage/events/button";
    };

    struct ChannelState
    {
        // 0=Undefined, 1=Idle, 2=PumpingManual, 3=PumpingAuto
        int stateCh1 = 0;
        int stateCh2 = 0;
    };

    MqttEvents(PublishFn publish, Config config);

    /// Compare against the previously-seen state and publish one event
    /// per channel transition. The very first call silently baselines —
    /// no event is fired for "boot state". Returns number of events
    /// published (publish failures count as 0).
    int updatePumpStates(const ChannelState &current, unsigned long nowMs);

    /// Publish a button event for a confirmed debounced press.
    /// `button` is one of "pump1", "pump2", "cancel". Returns whether
    /// the publish succeeded.
    bool publishButton(const std::string &button, unsigned long nowMs);

    /// Reset the baseline — call after MQTT disconnect/reconnect so
    /// the next `updatePumpStates` baselines silently instead of
    /// emitting potentially-stale transition events.
    void resetBaseline();

    /// Payload builders, exposed for tests.
    static std::string buildPumpPayload(int channel, int fromState, int toState,
                                        unsigned long tsMs);
    static std::string buildButtonPayload(const std::string &button,
                                          unsigned long tsMs);

private:
    PublishFn _publish;
    Config _config;
    bool _initialized = false;
    ChannelState _last;
};

#endif
