# Secrets

Persistent credential storage for WiFi + MQTT, backed by an injected
key-value store and seeded from build-time macros.

Build-time macros are the "factory default": when the firmware sees a
non-empty macro value that differs from what the store holds, it writes
the macro value into the store. Empty macros leave the store alone.
After seeding, all getters read from the store — so the same firmware
running on two devices with different NVS contents will use different
credentials, and later runtime mechanisms (provisioning, MQTT-driven
config) can rewrite NVS without a code change.

## Why this layer exists

For the bake-into-firmware alternative, see PR #47 commit message and
the discussion that landed there. Short version: keeping credentials in
NVS preserves the option to update them at runtime later (captive
portal, BLE provisioning, MQTT command) without re-flashing — important
for a sealed solar-powered outdoor box that's expensive to physically
reach.

## API

```cpp
#include "Secrets.h"

Secrets::KvStore store{
    [](const std::string& key) -> std::string { /* read */ },
    [](const std::string& key, const std::string& value) { /* write */ },
};

Secrets::BuildTimeValues build{
    /* wifiSsid    */ "MyNetwork",
    /* wifiPass    */ "MyPassword",
    /* mqttUser    */ "giessanlage",
    /* mqttPass    */ "changeme",
    /* mqttBroker  */ "192.168.1.10",
};

Secrets secrets(std::move(store), build);

secrets.wifiSsid();        // -> std::string
secrets.wifiPass();
secrets.mqttUser();
secrets.mqttPass();
secrets.mqttBroker();
secrets.hasCredentials();  // true iff wifiSsid is non-empty
```

The class is free of Arduino headers; it compiles in `env:native` and
the tests run on the host.

## Seeding rules

Per key, on construction:

| Build-time macro | Store currently holds | Action |
|---|---|---|
| empty           | anything           | leave store alone |
| non-empty       | same value         | leave store alone (idempotent) |
| non-empty       | different value    | write macro to store (reseed) |

After construction, getters always return what's in the store.

## Wiring on Arduino (`src/main.cpp`)

The `KvStore` lambdas wrap ESP32 `Preferences`:

```cpp
#include <Preferences.h>
Preferences prefs;

void setup() {
  prefs.begin("giessanlage", /*readOnly=*/false);

  Secrets::KvStore store{
      [](const std::string& k) {
          return std::string(prefs.getString(k.c_str(), "").c_str());
      },
      [](const std::string& k, const std::string& v) {
          prefs.putString(k.c_str(), v.c_str());
      },
  };

  Secrets::BuildTimeValues build{
      WIFI_SSID, WIFI_PASS, MQTT_USER, MQTT_PASS, MQTT_BROKER
  };

  auto secrets = new Secrets(std::move(store), build);
}
```

Macro values come from `platformio.ini`'s `[secrets]` section via
`build_flags`. Real values live in a gitignored `secrets.ini` — see
`secrets.ini.example`.

## Testing

Stub the store with a `std::map<std::string,std::string>` and a put
counter; assert seed behavior and idempotency. See
`test/test_secrets/test_secrets.cpp` for the six cases that cover the
rules above.

## Related issues

- #29 — closed by PR #47. Original spec for the secrets flow.
- #23 — WiFi connectivity, depends on this.
- MQTT issues (#24, #25, #26) — will use `mqttUser` / `mqttPass` / `mqttBroker`.
