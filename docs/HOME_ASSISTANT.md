# Home Assistant Integration

The controller integrates with Home Assistant purely over MQTT — no custom
component, no cloud, no HA REST calls from the device. HA auto-creates all
entities from a single retained **MQTT discovery** message, and a Lovelace
dashboard presents them.

The config lives in [`ha/`](../ha/):

| File | What it is |
|---|---|
| `ha/giessanlage_discovery.json` | Device-based MQTT discovery payload — defines every entity, grouped as one "Gießanlage" device, wired to the topics in [`MQTT.md`](MQTT.md). |
| `ha/giessanlage_dashboard.json` | Lovelace dashboard config (storage mode). |
| `ha/apply_ha.js` | Node helper that publishes the discovery message and creates/updates the dashboard. |

## Entities

All grouped under the **Gießanlage** device (`Settings → Devices`). Every
entity uses `giessanlage/availability` (the LWT topic) for availability, so
they show *unavailable* whenever the device is offline.

| Entity | Type | Source topic | Notes |
|---|---|---|---|
| Pump 1 / Pump 2 | `switch` | `commands/chN` + `status` | Toggles the pump; real state read back from `status`. |
| Pump 1 / 2 duration | `number` (s) | `commands/chN` (`set_timer`) + `status` | Per-channel run time. |
| Watering interval | `number` (h) | `config` + `status` | Rewrites the full retained `config` payload (reconstructs the two pump times from the duration entities). |
| Pump 1 / 2 state | `sensor` | `status` | `Idle` / `PumpingManual` / `PumpingAuto`. |
| Pump 1 / 2 remaining | `sensor` (s) | `status` | Countdown while pumping. |
| Next watering in | `sensor` (min) | `status` | Remaining interval, whole minutes. |
| Next watering countdown | `sensor` | `status` | Same value formatted `HH:mm:ss` from `remaining_watering_ms`. |
| Uptime | `sensor` (s) | `status` | Device uptime (diagnostic). |
| Online | `binary_sensor` | `availability` | Connectivity (diagnostic). |

> Display cadence follows the device's status publishing (see
> [`MQTT.md`](MQTT.md)): ~30 s heartbeat while idle, so countdowns step
> rather than tick every second.

## Applying the config

Prerequisites: HA reachable on the LAN with the MQTT integration set up
against the same broker the device uses, and a **long-lived access token**
(`Profile → Security → Long-lived access tokens`). Keep the token in a
gitignored `.env` at the repo root:

```ini
HATOKEN=eyJ...your-token...
```

Then, from the repo root:

```bash
set -a; . ./.env; set +a          # load HATOKEN (bash)
node ha/apply_ha.js               # publishes discovery + creates/updates the dashboard
```

Override the host or dashboard slug via env if needed:

```bash
HA_HOST=10.0.0.5:8123 DASH_URL=giessanlage-watering node ha/apply_ha.js
```

The script is idempotent — re-run it after editing either JSON to push the
changes. The dashboard is *storage mode*, so it's also editable in the HA UI
(open it → pencil, top-right); to keep the repo authoritative, re-export UI
edits back into `giessanlage_dashboard.json`.

## How the discovery path works

The device itself does **not** publish HA discovery yet (that's the firmware
follow-up, issue #27). Instead `apply_ha.js` calls HA's `mqtt.publish`
service so HA publishes the retained discovery message on its own broker
connection — which means **no broker credentials are needed** (nothing from
`secrets.ini`). HA's own discovery listener then creates the entities.

## Removing everything

Publish an empty payload to the discovery topic (clears the retained
message and removes all entities), then delete the dashboard in the HA UI
(`Settings → Dashboards → Gießanlage → delete`):

```bash
set -a; . ./.env; set +a
curl -s -X POST -H "Authorization: Bearer $HATOKEN" -H "Content-Type: application/json" \
  -d '{"topic":"homeassistant/device/giessanlage/config","payload":"","retain":true}' \
  http://192.168.50.100:8123/api/services/mqtt/publish
```
