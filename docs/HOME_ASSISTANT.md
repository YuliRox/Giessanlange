# Home Assistant Integration

The controller integrates with Home Assistant purely over MQTT — no custom
component, no cloud, no HA REST calls from the device. HA auto-creates all
entities from a single retained **MQTT discovery** message, and a Lovelace
dashboard presents them.

The config lives in [`ha/`](../ha/):

| File | What it is |
|---|---|
| `ha/giessanlage_discovery.json` | Device-based MQTT discovery payload — defines every entity, grouped as one "Gießanlage" device, wired to the topics in [`MQTT.md`](MQTT.md). |
| `ha/giessanlage_dashboard.json` | Lovelace dashboard config — the source of truth. |
| `ha/giessanlage_dashboard.yaml` | **Generated** from the JSON for YAML-mode installs. Do not hand-edit. |
| `ha/gen_dashboard_yaml.py` | Regenerates the YAML from the JSON and asserts they round-trip identically. |
| `ha/apply_ha.js` | Node helper that publishes the discovery message and creates/updates the dashboard. |

Every entity pins its `entity_id` via `default_entity_id` in the discovery
payload. Without it HA derives the id from the device's friendly name, which
drifts when the device is renamed or moved to an area — and a drifted id
silently breaks the cross-entity references in the `config` command templates
below. Note this only applies when an entity is **first** created; an entity
already in the registry keeps its existing id, so a drifted one must be
renamed by hand in the HA UI.

## Entities

All grouped under the **Gießanlage** device (`Settings → Devices`). Every
entity uses `giessanlage/availability` (the LWT topic) for availability, so
they show *unavailable* whenever the device is offline.

| Entity | Type | Source topic | Notes |
|---|---|---|---|
| Pump 1 / Pump 2 | `switch` | `commands/chN` + `status` | Toggles the pump; real state read back from `status`. |
| Pump 1 / 2 duration | `number` (s) | `commands/chN` (`set_timer`) + `status` | Per-channel run time. |
| Watering interval | `number` (h) | `config` + `status` | Rewrites the full retained `config` payload (reconstructs the two pump times *and* the pause flag from the other entities). |
| Pause watering | `switch` | `config` + `status` | `ON` = automatic watering suppressed. Manual pump control keeps working. |
| Pump 1 / 2 state | `sensor` | `status` | `Idle` / `PumpingManual` / `PumpingAuto`. |
| Pump 1 / 2 remaining | `sensor` (s) | `status` | Countdown while pumping. |
| Next watering in | `sensor` (min) | `status` | Remaining interval, whole minutes. |
| Next watering countdown | `sensor` | `status` | Same value formatted `HH:mm:ss` from `remaining_watering_ms`. |
| Uptime | `sensor` (s) | `status` | Device uptime (diagnostic). |
| Online | `binary_sensor` | `availability` | Connectivity (diagnostic). |

> Display cadence follows the device's status publishing (see
> [`MQTT.md`](MQTT.md)): ~30 s heartbeat while idle, so countdowns step
> rather than tick every second. Pump state and the pause flag are
> exceptions — those publish within ~1 s of changing, so toggles feel
> immediate rather than waiting out the idle interval.

### The two `config` writers must preserve each other

`giessanlage/config` is a **whole-document** topic: the device replaces its
config with whatever the payload contains, and any field left out falls
back to its default. Both entities that write it therefore reconstruct the
fields they don't own from current HA state:

- **Watering interval** reads the two duration entities *and* the pause
  switch.
- **Pause watering** reads the two duration entities *and* the interval.

Leaving `paused` out of the interval entity's template would silently clear
an active pause every time someone nudged the schedule — the failure would
be invisible until the beds got watered during a frost or a leak repair.
If you add a third writer to this topic, it must carry every field too.

Both also publish with `retain: true`. The device treats the retained
`config` message as broker truth on reconnect, so a non-retained write
would apply live and then be undone by the stale retained payload the next
time the device reconnected.

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
changes.

### Storage mode vs YAML mode

`apply_ha.js` can only write the dashboard if it is a **storage-mode**
dashboard. HA rejects `lovelace/config/save` on YAML-mode dashboards with a
bare `Not supported`, so the script now checks the mode first and tells you
what to do instead of failing obscurely. Discovery is published either way —
that path is unaffected.

If the dashboard is **storage mode**, the script writes it directly, and it
is also editable in the HA UI (open it → pencil, top-right); to keep the repo
authoritative, re-export UI edits back into `giessanlage_dashboard.json`.

If the dashboard is **YAML mode** — which is how the current instance is set
up, all three of its dashboards are — copy the generated file in instead.

On that instance HA runs in Docker on `192.168.50.100`, `/config` is
bind-mounted from `/home/ubuntu/homeassistant/config`, and
`configuration.yaml` already points `giessanlage-watering` at
`dashboards/giessanlage.yaml`. So:

```bash
python3 ha/gen_dashboard_yaml.py                       # regenerate from the JSON
scp ha/giessanlage_dashboard.yaml 192.168.50.100:/tmp/g.yaml
ssh 192.168.50.100 'cd /home/ubuntu/homeassistant/config/dashboards \
  && sudo cp -a giessanlage.yaml "giessanlage.yaml.bak-$(date +%Y%m%d-%H%M%S)" \
  && sudo install -o root -g root -m 644 /tmp/g.yaml giessanlage.yaml'
```

YAML dashboards are read on demand, so the change is live on the next page
load — no *Reload Lovelace* and no restart needed. The files are owned by
`root`, hence the `sudo`; back up first, since this overwrites whatever is
there and YAML dashboards are not editable in the UI to recover from.

Edit the **JSON** and regenerate; never hand-edit the YAML. The generator
asserts the two round-trip identically, so a mangled template fails loudly
rather than shipping a broken dashboard.

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
