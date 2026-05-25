# MqttConfig

Reconciles the per-device watering config across NVS (the device-local
source of truth when offline) and the broker's retained
`giessanlage/config` topic (the source of truth when reachable). The
device never writes its own config locally — only the broker writes.
The device reflects.

## Why this layer exists

- **Operates offline.** Pumps need correct durations even when the AP
  is down. NVS holds the last-known-good values across reboots; the
  state machine here loads them at boot and applies them to
  Giessanlage *before* MQTT is even attempted.
- **Broker-wins-when-reachable.** Once MQTT connects and the retained
  config message arrives, broker values take precedence and are
  persisted to NVS. Empty broker → device republishes NVS as the new
  retained config so the broker now reflects truth.
- **Validates.** Bad config from the broker (`pump_time = 0`, `pump_time
  >= watering_interval`, malformed JSON, missing fields) is rejected
  silently; the device keeps running on whatever was previously valid.
- **Testable.** All decisions live in pure C++ — six tests exercise
  every documented rule on the host.

## Schema

Retained `giessanlage/config` payload, JSON:

```json
{
  "pump_time_ch1_ms": 30000,
  "pump_time_ch2_ms": 45000,
  "watering_interval_ms": 86400000
}
```

NVS keys (in the bridge's chosen Preferences namespace — see
`src/main.cpp`, which uses `giessanlage_cfg`):

| Key | Type |
|---|---|
| `pump_time_ch1_ms` | unsigned long, as decimal string |
| `pump_time_ch2_ms` | unsigned long, as decimal string |
| `watering_interval_ms` | unsigned long, as decimal string |

## Validation rules

A config is **valid** iff:

- `pump_time_ch1_ms > 0`
- `pump_time_ch2_ms > 0`
- `watering_interval_ms > 0`
- `pump_time_ch1_ms < watering_interval_ms`
- `pump_time_ch2_ms < watering_interval_ms`

Invalid payloads are silently ignored (the bridge can layer logging on
top).

## Boot + reconnect sequence

```
   setup()
     │
     ▼
   initFromNvs()
     │   load NVS; if empty, seed defaults + persist
     │   apply values to Giessanlage via ApplyFn
     ▼
   (Giessanlage now uses NVS-loaded config)

   (later, after MQTT connects + subscribes)

   onMqttConnected()
     │   clears the "broker has spoken in this session" flag
     ▼
   broker sends retained config? ─── yes ──►  onConfigPayload(json)
     │                                          │
     │                                          ├── invalid? drop
     │                                          ├── matches NVS? no-op
     │                                          └── differs? broker wins:
     │                                                 - persist to NVS
     │                                                 - apply to Giessanlage
     │
     no
     │
     ▼ (after grace period, e.g. 3 s)
   publishIfBrokerSilent()
     │   publish current NVS values as retained config
     │   so the broker now reflects truth
```

## API

```cpp
MqttConfig config(
    /*store=*/ { /* get / put lambdas wrapping Preferences */ },
    /*publish=*/ [](const std::string& t, const std::string& p, bool r) { ... },
    /*apply=*/  [](const MqttConfig::Values& v) {
        anlage.setPumpTime(Channel::One, v.pumpTimeCh1Ms);
        anlage.setPumpTime(Channel::Two, v.pumpTimeCh2Ms);
        anlage.setWateringInterval(v.wateringIntervalMs);
    },
    /*config=*/ {
        "giessanlage/config",
        { /*pumpTimeCh1Ms*/ INTERVAL_30S, /*pumpTimeCh2Ms*/ INTERVAL_30S,
          /*wateringIntervalMs*/ INTERVAL_24H }
    }
);

config.initFromNvs();   // call once at startup

// after MQTT (re)connect:
config.onMqttConnected();

// in MQTT message callback for the config topic:
config.onConfigPayload(payloadString);

// after grace period:
if (!config.brokerHasSpoken())
    config.publishIfBrokerSilent();
```

## Tests

`test/test_mqtt_config/test_mqtt_config.cpp`, 7 cases — one per
documented rule plus an explicit reconnect-resets-broker-seen check:

- First boot, no NVS → seeds defaults + writes NVS
- Second boot with NVS → no writes
- Broker config matches NVS → no-op
- Broker config differs → broker wins, NVS updated, ApplyFn called
- Broker silent after grace → publishes NVS as retained
- Invalid payloads (zero, too-large, malformed, missing field) rejected
- Reconnect resets the broker-spoken flag

## Implementation notes

- **Hand-rolled JSON parser.** Three integer fields, order-tolerant
  via per-field substring search. Avoids pulling ArduinoJson into the
  lib component; main.cpp uses ArduinoJson for other concerns but this
  lib stays standalone.
- **Separate Preferences namespace.** The bridge wires NVS into a
  distinct namespace (`giessanlage_cfg`) so a Secrets reseed cannot
  clobber config keys. Each concern owns its own NVS partition slice.

## Related issues

- #26 — original spec for this component.
- #24 — MqttStatus republishes when config changes invalidate the
  retained snapshot.
- #27 — HomeAssistant discovery payload includes the configurable
  durations as `number` entities pointing at this topic.
