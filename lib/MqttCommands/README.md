# MqttCommands

Applies remote pump/timer commands from a per-channel MQTT topic
(`giessanlage/commands/ch1`, `giessanlage/commands/ch2`) directly to a
`Giessanlage` instance.

## Why per-channel topics

The channel is already implicit in which topic a message arrives on,
so the JSON payload only needs to carry the verb (and a duration where
relevant) — no redundant channel field to keep in sync with the topic.

## Schema

```json
{"cmd":"toggle_pump"}
{"cmd":"set_timer","time_ms":30000}
{"cmd":"reset_timer","time_ms":30000}
```

- `toggle_pump` — `stopPump()` if the channel is currently pumping,
  else `triggerPump()`.
- `set_timer` — `setPumpTime()` only. Like the existing
  `giessanlage/config` topic, this does **not** restart an in-flight
  countdown.
- `reset_timer` — `setPumpTime()` then `resetPumpTimer()`, restarting
  the in-flight countdown to the new duration. Same bug class fixed in
  the `giessanlage/config` path (see `resetWateringTimer()`): a setter
  alone doesn't touch a running countdown.

Malformed JSON, an unknown `cmd`, or a missing `time_ms` for
`set_timer`/`reset_timer` is rejected without any `Giessanlage` call.

## Rate limiting

Each channel gets its own 200ms rate-limit slot, so a flood of commands
on one channel can't starve the other.

## Tests

`test/test_mqtt_commands/test_mqtt_commands.cpp` drives a real
`Giessanlage` instance directly (no fakes needed).
