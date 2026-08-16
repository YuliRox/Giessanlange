# MqttStatus

Builds and publishes the retained device status to `giessanlage/status`.
Carries the after-state of every transition so a consumer subscribing
fresh to the broker can reconstruct what the device is doing without
listening to the live event stream.

## Why a separate component

- **Throttling and change detection in one place.** The state machine
  may transition multiple times within a single 100 ms loop tick (e.g.
  pump auto-fires then immediately rejected by an interlock). Without
  coalescing we'd burn broker QoS slots on intermediate states no
  consumer cares about.
- **Testable independently of the transport.** Native unit tests cover
  payload schema, throttle, and invalidate-after-reconnect without
  needing a real MQTT broker or Arduino runtime.
- **Reusable by the display** (#41). The same Snapshot struct that
  feeds the JSON builder can feed an e-paper redraw — both consume the
  same domain object.

## API

```cpp
#include "MqttStatus.h"

MqttStatus::Snapshot s;
s.stateCh1 = static_cast<int>(anlage.getState(Channel::One));
// ... fill remaining fields ...

MqttStatus status(
    /*publish=*/ [](const std::string& topic,
                    const std::string& payload,
                    bool retained) -> bool {
        return mqttClient.publish(topic.c_str(), payload.c_str(), retained);
    },
    MqttStatus::Config{}            // defaults: "giessanlage/status", 1000 ms throttle
);

// in loop():
status.update(s, nowMs);            // publishes iff state changed AND >= minIntervalMs
status.invalidate();                // call after MQTT reconnect to force one publish
```

## Payload schema

JSON, all fields always present. State enums serialised as strings.

```json
{
  "state_ch1": "Idle|PumpingManual|PumpingAuto",
  "state_ch2": "Idle|PumpingManual|PumpingAuto",
  "remaining_pump_ms_ch1": 0,
  "remaining_pump_ms_ch2": 0,
  "remaining_watering_ms": 0,
  "pump_time_ch1_ms": 30000,
  "pump_time_ch2_ms": 30000,
  "watering_interval_ms": 86400000,
  "paused": false,
  "uptime_ms": 0
}
```

`paused` mirrors the config flag that suppresses automatic watering, so a
consumer can show the current state and detect drift.

`buildPayload(snapshot)` is `static` and exposed for tests / display
debugging.

## Update rules

| Condition | Action |
|---|---|
| First call ever | Publish |
| Snapshot same as last *successfully* published | No-op |
| Snapshot differs, but `< minIntervalMs` since last publish | No-op (coalesced) |
| Snapshot differs, `>= minIntervalMs` elapsed | Publish, record |
| Only countdown fields differ while all channels are idle | Throttled to `idleIntervalMs` (5 min) instead |
| `stateCh1`/`stateCh2`/`paused` differs | Always `minIntervalMs`, even when the new snapshot is idle |
| Nothing meaningful differs, `>= heartbeatIntervalMs` elapsed | Publish (keeps the retained snapshot fresh) |
| Publish fails (broker offline) | Return false; do NOT record as published — retry on next call |
| `invalidate()` called | Next non-throttled call publishes regardless of equality |

The retained flag is always `true` — last write wins on the broker, so
late subscribers always see the current state.

The wide idle floor exists because `remainingWateringMs` counts down on
nearly every tick, so idle status would otherwise publish at
`minIntervalMs` cadence forever. It deliberately does **not** cover
discrete edges — channel state transitions or `paused`. Applying it to
the edge into `Idle` delayed the "pump finished" publish by up to 5
minutes (#66), which made switch on-time useless as a runtime measure;
`paused` is excluded for the same reason, since it is only ever changed
while idle and a consumer would otherwise show stale state for as long.

## Wiring on Arduino

In `src/main.cpp`:

```cpp
PubSubClient mqttClient(wifiClient);
MqttStatus* mqttStatus = nullptr;

void setup() {
    // ... after MQTT setServer / setCallback ...
    mqttStatus = new MqttStatus(
        [](const std::string& topic, const std::string& payload, bool retained) {
            if (!mqttClient.connected()) return false;
            return mqttClient.publish(topic.c_str(), payload.c_str(), retained);
        },
        {}
    );
}

// after MQTT (re)connects:
mqttStatus->invalidate();   // forces the next update() to publish

// in loop() once MQTT is connected:
MqttStatus::Snapshot s = { /* fill from anlage.getState(...) etc. */ };
mqttStatus->update(s, millis());
```

## Tests

`test/test_mqtt_status/test_mqtt_status.cpp`, 7 cases:

- Payload schema fields + values
- First call publishes
- Unchanged snapshot is a no-op
- Throttle holds back rapid changes within the window
- Publish failure is not recorded (retries on next call)
- `invalidate()` forces republish even when unchanged
- Changed snapshot after throttle window publishes

## Related issues

- #24 — original spec for this component.
- #25 — MqttEvents publishes alongside, never replaces.
- #41 — the e-paper status screen will consume the same Snapshot.
