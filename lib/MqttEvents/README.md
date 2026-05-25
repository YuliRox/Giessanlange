# MqttEvents

Live event stream over MQTT: one event per confirmed state transition
or button press. Non-retained, fire-and-forget — the retained status
snapshot (see `lib/MqttStatus/`) carries the after-state for late
subscribers, so dropped events on reconnect are not a leak.

## Topics

```
giessanlage/events/pump      (not retained, QoS 0)
giessanlage/events/button    (not retained, QoS 0)
```

## Why this is split from MqttStatus

- Different semantics: status is a snapshot, events are diffs.
- Different retain policy: status is retained, events are not.
- Different consumers: a HomeAssistant automation subscribes to events
  to trigger downstream actions; a dashboard widget consumes status.
- Different drop tolerance: missed events are acceptable (the after-
  state is in status); a missed status snapshot is not.

## Payloads

Pump event (one per channel transition):

```json
{ "channel": 1, "from": "Idle", "to": "PumpingManual", "ts_ms": 1234567 }
```

Button event (one per confirmed debounced press):

```json
{ "button": "pump1|pump2|cancel", "ts_ms": 1234567 }
```

`ts_ms` is `millis()` at the publishing site. When NTP lands (#37) the
bridge can swap to wall-clock — payload format unchanged.

## API

```cpp
MqttEvents ev(
    /*publish=*/ [](const std::string& t, const std::string& p, bool r) {
        return mqttClient.publish(t.c_str(), p.c_str(), r);
    },
    MqttEvents::Config{}      // defaults: giessanlage/events/{pump,button}
);

// in loop(), after Giessanlage::tick():
MqttEvents::ChannelState st {
    static_cast<int>(anlage.getState(Channel::One)),
    static_cast<int>(anlage.getState(Channel::Two)),
};
ev.updatePumpStates(st, nowMs);   // diffs, publishes 0..2 events

// on confirmed debounced button:
if (buttonPump1.poll(nowMs)) {
    togglePump(Channel::One);
    ev.publishButton("pump1", nowMs);
}

// after MQTT reconnect:
ev.resetBaseline();               // next updatePumpStates baselines silently
```

## Baseline + drop-on-offline rules

| Condition | Action |
|---|---|
| First `updatePumpStates` call (or after `resetBaseline()`) | Silently baseline; no event |
| Subsequent call, same state | No-op |
| Subsequent call, different state on ch1 / ch2 | Publish one event per changed channel |
| Publish fails (broker offline) | Drop event silently; state still advances internally so we don't loop-retry |
| `publishButton` while offline | Returns false, no buffering |

Crucial subtlety: state advances even on a failed publish. Without
that, a one-off broker hiccup at the moment of a transition would
queue the event indefinitely; instead the retained status snapshot
(MqttStatus) eventually carries the after-state when the broker is
back, and the event stream silently lost the live notification.
That's the documented tradeoff.

## Tests

`test/test_mqtt_events/test_mqtt_events.cpp`, 7 cases:

- First call baselines silently
- Single-channel transition emits one event
- Simultaneous two-channel transitions emit two events
- No-change is a no-op
- Button event payload schema + non-retained
- Offline drops events silently and keeps advancing state
- `resetBaseline()` silences the next call (covers reconnect path)

## Wiring on Arduino

The bridge in `src/main.cpp` calls `updatePumpStates` once per loop
tick after `anlage.tick()`, and threads `publishButton` calls into
the existing `DebouncedButton::poll()` true-returns. On every
successful MQTT reconnect it calls `resetBaseline()` so the next
state diff doesn't replay transitions that happened while offline.

## Related issues

- #25 — original spec for this component.
- #24 — MqttStatus carries the after-state when events are dropped.
- #38 — sensor events (moisture / tank level) will probably layer on
  the same component or a sibling once that work lands.
