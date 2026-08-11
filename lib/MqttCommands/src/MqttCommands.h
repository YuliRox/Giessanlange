#ifndef MqttCommands_h
#define MqttCommands_h

#include <string>

#include "Giessanlage.h"

/// Dispatches JSON commands arriving on a per-channel MQTT commands topic
/// (`giessanlage/commands/ch1`, `giessanlage/commands/ch2`) directly onto a
/// Giessanlage instance. The channel is determined by the caller from which
/// topic the message arrived on — the payload itself carries only the verb
/// and (where relevant) a duration.
///
/// Payloads:
///   {"cmd":"toggle_pump"}
///   {"cmd":"set_timer","time_ms":30000}
///   {"cmd":"reset_timer","time_ms":30000}
///
/// - toggle_pump: stopPump() if currently pumping, else triggerPump().
/// - set_timer: setPumpTime() only — does not restart an in-flight countdown,
///   matching the existing giessanlage/config semantics.
/// - reset_timer: setPumpTime() then resetPumpTimer() — restarts the
///   in-flight countdown to the new duration.
///
/// Malformed JSON, an unknown cmd, or a missing time_ms for set_timer /
/// reset_timer is rejected without touching Giessanlage. Each channel has an
/// independent 200ms rate-limit slot so a flood on one channel cannot starve
/// the other.
class MqttCommands
{
public:
    /// Returns true if a command was parsed, valid, and applied (i.e. not
    /// dropped for being malformed or rate-limited).
    bool dispatch(Giessanlage &g, Giessanlage::Channel channel,
                  const std::string &jsonPayload, unsigned long nowMs);

private:
    static constexpr unsigned long RATE_LIMIT_MS = 200;
    static constexpr int CHANNEL_COUNT = 2;

    unsigned long _lastDispatchMs[CHANNEL_COUNT] = {0, 0};
    bool _hasDispatched[CHANNEL_COUNT] = {false, false};

    static int idx(Giessanlage::Channel c) { return static_cast<int>(c); }
};

#endif
