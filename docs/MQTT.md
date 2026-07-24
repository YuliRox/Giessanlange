# MQTT Integration

The controller talks to a local MQTT broker for **status**, **events**,
and **config sync** (and, separately, remote **commands** — see
`docs/MQTT_COMMANDS.md`). The device publishes what it's doing and
reflects config the broker owns; it never treats itself as the config
authority when the broker is reachable.

Each concern is a platform-independent, unit-tested `lib/` component
wired up in `src/mqtt_glue.cpp`:

| Concern | Component | This doc | Deeper reference |
|---|---|---|---|
| Availability (LWT) | — (`mqtt_glue.cpp`) | below | — |
| Status snapshot | `MqttStatus` | below | `lib/MqttStatus/README.md` |
| Live events | `MqttEvents` | below | `lib/MqttEvents/README.md` |
| Config sync | `MqttConfig` | below | `lib/MqttConfig/README.md` |
| Remote commands | `MqttCommands` | `docs/MQTT_COMMANDS.md` | `lib/MqttCommands/` |

## Prerequisites

Set `mqtt_broker`, `mqtt_user`, `mqtt_pass` in `secrets.ini` (copy from
`secrets.ini.example`). The device connects to the broker on port
`1883` with client ID `giessanlage` once WiFi is up, and reconnects
automatically (5 s backoff). If `mqtt_broker` is empty, the whole MQTT
layer stays dormant and the device runs standalone on its NVS-stored
config.

## Topic overview

| Topic | Direction | Retained | Payload |
|---|---|---|---|
| `giessanlage/availability` | device → broker | ✅ | `online` / `offline` |
| `giessanlage/status` | device → broker | ✅ | JSON snapshot |
| `giessanlage/events/pump` | device → broker | ❌ | JSON, one per pump transition |
| `giessanlage/events/button` | device → broker | ❌ | JSON, one per button press |
| `giessanlage/config` | **both** | ✅ | JSON config |
| `giessanlage/commands/ch1` | broker → device | ❌ | see `docs/MQTT_COMMANDS.md` |
| `giessanlage/commands/ch2` | broker → device | ❌ | see `docs/MQTT_COMMANDS.md` |

## Availability (Last Will & Testament)

On connect the device publishes a retained `online` to
`giessanlage/availability`, and registers a retained-LWT `offline` on
the same topic. If the device drops off the network ungracefully, the
broker publishes `offline` on its behalf, so a dashboard or Home
Assistant `availability_topic` always reflects reachability without
polling.

## Status snapshot (`giessanlage/status`, retained)

The after-state of everything the device is doing. Retained, so a
late subscriber reconstructs the current state without replaying the
event stream. All fields always present; state enums are strings.

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
  "uptime_ms": 0
}
```

**Publish cadence** (see `MqttStatus::Config`):

- A meaningful state change publishes at most once per **1 s** while a
  channel is pumping (coalesces bursts of transitions).
- While **all channels are idle**, publishes are held to at most once
  per **5 min** — `remaining_watering_ms` counts down every tick, so
  without a wider idle floor the snapshot would look "changed" forever.
- Even with nothing meaningful changing, a **30 s heartbeat** keeps the
  retained snapshot fresh.
- After a reconnect the device force-republishes once, so the broker
  always sees current truth.

## Events (`giessanlage/events/*`, not retained)

Live diffs — one message per confirmed transition. Non-retained and
fire-and-forget: a dropped event is not a leak because the retained
status snapshot carries the after-state. Useful for triggering
downstream automations.

Pump event (one per channel transition):

```json
{ "channel": 1, "from": "Idle", "to": "PumpingManual", "ts_ms": 1234567 }
```

Button event (one per confirmed debounced press):

```json
{ "button": "pump1|pump2|cancel", "ts_ms": 1234567 }
```

`ts_ms` is device uptime (`millis()`) at publish time, not wall clock.

## Config sync (`giessanlage/config`, retained, bidirectional)

The one **bidirectional** topic. The broker is the source of truth when
reachable; NVS is the source of truth when offline. The device never
authors its own config while the broker is present — it reflects.

```json
{
  "pump_time_ch1_ms": 30000,
  "pump_time_ch2_ms": 45000,
  "watering_interval_ms": 86400000
}
```

Behavior:

- **Boot:** load config from NVS (seeding firmware defaults if empty)
  and apply to the state machine *before* WiFi/MQTT come up, so pumps
  have correct durations even offline.
- **Broker wins when reachable:** once connected, a retained
  `giessanlage/config` from the broker takes precedence — it's persisted
  to NVS and applied live. Applying config also **resets the in-flight
  watering countdown** to the new interval.
- **Broker silent:** if no retained config arrives within a ~3 s grace
  window, the device republishes its NVS values as the retained config,
  so the broker now reflects truth.
- **Validation:** a payload is accepted only if every field is `> 0`
  and each `pump_time_*` is `< watering_interval_ms`. Malformed JSON,
  missing fields, or out-of-range values are rejected silently and the
  device keeps running on the last valid config.

> Note the difference from the `reset_timer` command: a config update
> restarts the **watering** countdown, whereas `set_timer` (via the
> commands topic) changes a pump duration *without* restarting an
> in-flight pump run. See `docs/MQTT_COMMANDS.md`.

## Trying it out (mosquitto)

```bash
# Watch everything the device publishes
mosquitto_sub -h <broker> -u <user> -P <pass> -v -t 'giessanlage/#'

# Set both pump durations + the watering interval (broker wins, persisted to NVS)
mosquitto_pub -h <broker> -u <user> -P <pass> -r \
  -t giessanlage/config \
  -m '{"pump_time_ch1_ms":30000,"pump_time_ch2_ms":45000,"watering_interval_ms":86400000}'
```

(The `-r` retains the config so the device picks it up on its next
reconnect, not just while it's already online.)

## Notes / limits

- **No per-topic ACL.** Anyone with valid broker credentials can read
  status and write config/commands. Scope broker credentials to match
  your deployment's trust boundary.
- **Buffer size.** `PubSubClient`'s packet buffer is raised to 512 B in
  `mqtt_glue.cpp`; a full status snapshot with both channels pumping
  exceeds the stock 256 B buffer and would otherwise fail to publish
  silently. Keep that in mind if you add fields to the status payload.
- **Timestamps are uptime**, not wall clock, until NTP lands.
