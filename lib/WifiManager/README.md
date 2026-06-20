# WifiManager

Non-blocking WiFi reconnect state machine. Drives `WiFi.begin()` /
`WiFi.status()` through injected callables so the policy is platform-
independent and host-testable; the Arduino-side glue lives in
`src/main.cpp`.

## States

```
   ┌──────────────┐  backoff elapsed  ┌──────────────┐
   │ Disconnected │ ─────────────────>│  Connecting  │
   └──────────────┘                   └──────────────┘
          ▲                              │       │
          │ probe says false (drop)      │       │ timeout
          │                              │       │ (backoff doubles)
          │                              │       ▼
          │        ┌──────────────┐      │  Disconnected
          └──────  │   Connected  │ <────┘
                   └──────────────┘  probe says true
```

Empty SSID → manager parks in `Disconnected` and never fires
`beginConnect`. Lets a freshly flashed device boot and run pumps
without complaining about missing credentials.

## Why this exists (vs. bare `WiFi.begin()`)

Real benefits:

1. **Testable policy** — the "when do I retry, what's the backoff" logic
   compiles and runs in `env:native`; 6 unit tests cover the edges.
2. **Edge events** — `tick()` returns `true` exactly once per state
   transition. Clean hook for "log the IP once on connect", "trigger
   MQTT reconnect on WiFi up", "redraw status icon on drop".
3. **Sleep predictability** — owning the state machine means we control
   when `WiFi.begin()` re-fires post-wake (#26), rather than fighting
   the framework's implicit auto-reconnect.

What it does NOT replace: the framework's own internal retry during a
single `begin()` call, or `setAutoReconnect(true)` for transient
association loss. Those still apply; the manager observes them.

## API

```cpp
#include "WifiManager.h"

WifiManager::Config cfg;            // optional; defaults are sensible
cfg.initialBackoffMs   = 2000UL;    // wait before first retry
cfg.maxBackoffMs       = 30UL*1000; // cap on exponential growth
cfg.connectTimeoutMs   = 10UL*1000; // how long one attempt gets

WifiManager wifi(
    /*beginConnect=*/ [](const std::string& ssid, const std::string& pass) {
        WiFi.begin(ssid.c_str(), pass.c_str());
    },
    /*isConnectedProbe=*/ []() { return WiFi.status() == WL_CONNECTED; },
    /*ssid=*/ secrets.wifiSsid(),
    /*password=*/ secrets.wifiPass(),
    cfg);

// in loop():
bool changed = wifi.tick(deltaMs);
if (changed) {
    // log / signal MQTT / redraw display ...
}

wifi.state();           // Disconnected | Connecting | Connected
wifi.isConnected();
wifi.failedAttempts();  // resets to 0 on Connected
```

## Wiring on Arduino (`src/main.cpp`)

Done in two phases — one-shot radio configuration in `setup()`, then
the manager runs the state machine in `loop()`:

```cpp
// in setup(), BEFORE constructing WifiManager:
WiFi.persistent(false);          // Secrets owns credential storage
WiFi.mode(WIFI_STA);             // station mode
WiFi.setHostname("giessanlage"); // appears under that name in DHCP
WiFi.setAutoReconnect(true);     // framework handles transient drops
```

The per-attempt `beginConnect` lambda only calls `WiFi.begin()` — mode
and hostname are already set.

## Backoff

Exponential, with cap. On every failed attempt the backoff doubles up
to `maxBackoffMs`. On a successful association the backoff resets to
`initialBackoffMs`. Failed attempts counter resets on Connected; query
via `failedAttempts()` for diagnostics or display.

## Empty-credentials behavior

Constructing with `ssid == ""` parks the manager in `Disconnected`
permanently. `tick()` returns false and `beginConnect` is never fired.
The device runs pumps and buttons normally — WiFi is best-effort.

## Testing

Inject a `WifiFake` that records `beginConnect` calls and exposes a
mutable `connected` boolean. Drive the manager with synthetic `tick()`
calls and assert state transitions + call counts. See
`test/test_wifi/test_wifi.cpp` for the six cases.

## Related issues

- #23 — WiFi connectivity. Spec for this component.
- #26 — deep sleep + RTC wake. Will need predictable WiFi tear-down
  before sleep and re-association after wake; the manager is the
  natural place to hang those hooks.
- MQTT issues (#24/#25/#26) — gate MQTT connect on `wifi.isConnected()`.
