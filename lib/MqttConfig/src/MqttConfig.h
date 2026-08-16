#ifndef MqttConfig_h
#define MqttConfig_h

#include <functional>
#include <string>

/// Reconciles the per-device watering config across NVS and the MQTT
/// broker's retained `giessanlage/config` topic. Broker is the source
/// of truth when reachable; NVS is the source of truth when offline.
/// The device never mutates its own config — only the broker writes;
/// MqttConfig reflects.
///
/// Boot sequence:
///   1. `initFromNvs()` loads NVS (or seeds defaults on first boot) and
///      applies the values to Giessanlage via the injected ApplyFn.
///   2. After MQTT connects + subscribes, the bridge calls
///      `onMqttConnected()` to reset the "have we heard from broker?"
///      tracking.
///   3a. If the broker delivers the retained `config` message, the
///       bridge calls `onConfigPayload(json)`. If parsing succeeds and
///       differs from NVS, broker wins: apply + persist.
///   3b. If no retained `config` arrives within the bridge's grace
///       period, the bridge calls `publishIfBrokerSilent()` which
///       publishes the NVS values as the retained `config` so the
///       broker now reflects truth.
class MqttConfig
{
public:
    struct Values
    {
        unsigned long pumpTimeCh1Ms = 0;
        unsigned long pumpTimeCh2Ms = 0;
        unsigned long wateringIntervalMs = 0;

        // Suppresses automatic watering only; manual pump control stays
        // available. Optional on the wire and absent from older NVS stores,
        // where it reads as false — see parsePayload() and initFromNvs().
        bool paused = false;

        bool operator==(const Values &o) const
        {
            return pumpTimeCh1Ms == o.pumpTimeCh1Ms &&
                   pumpTimeCh2Ms == o.pumpTimeCh2Ms &&
                   wateringIntervalMs == o.wateringIntervalMs &&
                   paused == o.paused;
        }
        bool operator!=(const Values &o) const { return !(*this == o); }
    };

    /// Same shape as Secrets::KvStore but declared locally to avoid the
    /// cross-lib dependency. The bridge wires both to the same
    /// Preferences instance on the Arduino side.
    struct KvStore
    {
        std::function<std::string(const std::string &)> get;
        std::function<void(const std::string &, const std::string &)> put;
    };

    using PublishFn = std::function<bool(const std::string &topic,
                                         const std::string &payload,
                                         bool retained)>;

    /// Called whenever the effective Values change. Implementation
    /// should call Giessanlage's per-channel setters + setWateringInterval.
    using ApplyFn = std::function<void(const Values &)>;

    struct Config
    {
        std::string topic = "giessanlage/config";
        Values defaults; // zeroed by default; bridge supplies real defaults
    };

    MqttConfig(KvStore store, PublishFn publish, ApplyFn apply, Config config);

    /// Step 1 of boot. Loads NVS, applies to Giessanlage. If NVS is
    /// empty (first boot ever), seeds defaults and writes them to NVS.
    Values initFromNvs();

    /// Called when MQTT (re)connects. Resets the broker-seen tracking
    /// so the next config-message arrival or grace expiry behaves
    /// correctly on reconnect.
    void onMqttConnected();

    /// Called when the broker delivers the retained config topic (or a
    /// runtime update). Returns true if the payload was valid and
    /// resulted in a state change (apply + persist). False otherwise.
    bool onConfigPayload(const std::string &jsonPayload);

    /// Called after a grace period if no retained `config` arrived from
    /// the broker. Publishes current NVS values as the new retained
    /// config so the broker now mirrors device state. Returns whether
    /// the publish succeeded.
    bool publishIfBrokerSilent();

    /// True once `onConfigPayload` has been called with a valid payload
    /// in the current MQTT session. The bridge uses this to decide
    /// whether to invoke `publishIfBrokerSilent()` after the grace
    /// period.
    bool brokerHasSpoken() const;

    Values currentValues() const;

    bool isValid(const Values &v) const;

    /// Exposed for tests.
    static std::string buildPayload(const Values &v);
    static bool parsePayload(const std::string &json, Values &out);

private:
    KvStore _store;
    PublishFn _publish;
    ApplyFn _apply;
    Config _config;

    Values _current;
    bool _brokerSeenThisSession = false;

    void persist(const Values &v);
};

#endif
