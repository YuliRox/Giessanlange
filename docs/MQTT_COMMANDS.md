# MQTT Remote Commands

The device listens on two topics for remote pump control — one per
channel — so pumps and their durations can be driven from the broker,
not just the physical buttons. The dispatcher lives in
`lib/MqttCommands/` (platform-independent, unit-tested) and is wired
up in `src/mqtt_glue.cpp`.

## Prerequisites

MQTT must already be configured and connected: `mqtt_broker`,
`mqtt_user`, `mqtt_pass` set in `secrets.ini` (see the main secrets
setup). No separate setup is needed for commands beyond that — the
device subscribes to both command topics automatically on every
successful broker connection.

## Topics

```
giessanlage/commands/ch1
giessanlage/commands/ch2
```

Not retained. Each topic controls one channel; the channel is
determined entirely by which topic you publish to, so the payload
itself never repeats the channel number. A stale retained message
would otherwise risk re-triggering a pump on reboot — publishing
without the retain flag avoids that.

## Payloads

Minimal JSON, one `cmd` field plus an optional `time_ms`:

| Command | Payload | Effect |
|---|---|---|
| Toggle pump | `{"cmd":"toggle_pump"}` | Starts the pump if idle, stops it if currently pumping. |
| Set timer | `{"cmd":"set_timer","time_ms":30000}` | Changes the configured pump duration. Does **not** restart an in-flight countdown — matches the existing `giessanlage/config` topic's behavior. |
| Reset timer | `{"cmd":"reset_timer","time_ms":30000}` | Changes the configured pump duration **and** restarts the in-flight countdown to the new value immediately. |

`time_ms` is required for `set_timer`/`reset_timer` and is milliseconds
(e.g. `30000` = 30 s). Malformed JSON, an unknown `cmd`, or a missing
`time_ms` where required is rejected silently on-device — no pump
state changes, nothing crashes.

## Examples (mosquitto_pub)

```bash
# Start pump 1
mosquitto_pub -h <broker> -u <user> -P <pass> \
  -t giessanlage/commands/ch1 -m '{"cmd":"toggle_pump"}'

# Stop pump 1 (same command — it's a toggle)
mosquitto_pub -h <broker> -u <user> -P <pass> \
  -t giessanlage/commands/ch1 -m '{"cmd":"toggle_pump"}'

# Change channel 2's configured pump duration to 45s (takes effect next run)
mosquitto_pub -h <broker> -u <user> -P <pass> \
  -t giessanlage/commands/ch2 -m '{"cmd":"set_timer","time_ms":45000}'

# Extend/shorten a pump that's running right now on channel 1 to 15s remaining
mosquitto_pub -h <broker> -u <user> -P <pass> \
  -t giessanlage/commands/ch1 -m '{"cmd":"reset_timer","time_ms":15000}'
```

## Rate limiting

Each channel enforces its own 200ms minimum gap between accepted
commands — a flood of retries or a misconfigured automation on one
channel can't starve the other. Commands arriving faster than that on
the same channel are dropped silently (same as a malformed payload:
no pump state change, no crash, nothing published back).

## Watching the result

There's no direct ack topic. Confirm a command took effect via the
existing status/event topics:

- `giessanlage/status` (retained) — updated promptly after a successful
  command; includes `state_ch1`/`state_ch2` and
  `remaining_pump_ms_ch1`/`_ch2`.
- `giessanlage/events/pump` (not retained) — fires on every pump state
  transition, whether triggered by a command, the physical button, or
  the automatic watering cycle.

## Safety notes

- Anyone who can publish to the broker with valid MQTT credentials can
  run the pumps — there's no separate ACL on the commands topics
  beyond the broker's own username/password. Scope broker credentials
  accordingly if this matters for your deployment.
- Commands are non-retained by design, so a stale "toggle_pump" isn't
  replayed and doesn't re-trigger a pump after a device reboot.
