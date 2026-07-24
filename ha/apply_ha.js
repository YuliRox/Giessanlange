// Apply the Gießanlage Home Assistant config: MQTT discovery + Lovelace dashboard.
//
// Publishes the device MQTT-discovery message via HA's mqtt.publish service
// (REST) and creates/updates the storage dashboard via HA's WebSocket API.
// No broker credentials needed — HA publishes on its own MQTT connection.
//
// Requires: Node >= 21 (built-in WebSocket), a running HA reachable on the LAN.
//
// Env:
//   HA_HOST   HA base host:port         (default 192.168.50.100:8123)
//   HATOKEN   long-lived access token   (required)
//   DASH_URL  dashboard url_path        (default giessanlage-watering, must contain a hyphen)
//
// Usage:
//   HATOKEN=xxxx node ha/apply_ha.js
//   HATOKEN=xxxx HA_HOST=10.0.0.5:8123 node ha/apply_ha.js
//
// (Tip: keep the token in a gitignored .env and `set -a; . ./.env; set +a` first.)

const fs = require("fs");
const path = require("path");

const HOST = process.env.HA_HOST || "192.168.50.100:8123";
const TOKEN = process.env.HATOKEN;
const URL_PATH = process.env.DASH_URL || "giessanlage-watering";
const DISCOVERY_TOPIC = "homeassistant/device/giessanlage/config";

if (!TOKEN) { console.error("ERROR: HATOKEN env var is required"); process.exit(2); }

const here = __dirname;
const discovery = JSON.parse(fs.readFileSync(path.join(here, "giessanlage_discovery.json"), "utf8"));
const dashboard = JSON.parse(fs.readFileSync(path.join(here, "giessanlage_dashboard.json"), "utf8"));

async function publishDiscovery() {
  const body = JSON.stringify({ topic: DISCOVERY_TOPIC, payload: JSON.stringify(discovery), retain: true, qos: 0 });
  const res = await fetch(`http://${HOST}/api/services/mqtt/publish`, {
    method: "POST",
    headers: { Authorization: `Bearer ${TOKEN}`, "Content-Type": "application/json" },
    body,
  });
  if (!res.ok) { console.error("discovery publish failed:", res.status, await res.text()); process.exit(3); }
  console.log("MQTT discovery published (retained) ->", DISCOVERY_TOPIC);
}

function saveDashboard() {
  return new Promise((resolve) => {
    const ws = new WebSocket(`ws://${HOST}/api/websocket`);
    let id = 0;
    const pending = new Map();
    const send = (msg) => { const myId = ++id; pending.set(myId, msg.type); ws.send(JSON.stringify({ id: myId, ...msg })); };

    ws.addEventListener("message", (ev) => {
      const m = JSON.parse(ev.data);
      if (m.type === "auth_required") return ws.send(JSON.stringify({ type: "auth", access_token: TOKEN }));
      if (m.type === "auth_invalid") { console.error("AUTH FAILED"); process.exit(4); }
      if (m.type === "auth_ok") {
        return send({ type: "lovelace/dashboards/create", url_path: URL_PATH, title: dashboard.title, icon: "mdi:watering-can", show_in_sidebar: true, require_admin: false });
      }
      if (m.type !== "result") return;
      const what = pending.get(m.id);
      if (what === "lovelace/dashboards/create") {
        const msg = m.error && m.error.message;
        if (m.success) console.log("dashboard created:", URL_PATH);
        else if (/already|in use|exists/i.test(msg || "")) console.log("dashboard exists — updating config");
        else { console.error("CREATE FAILED:", m.error); process.exit(5); }
        return send({ type: "lovelace/config/save", url_path: URL_PATH, config: dashboard });
      }
      if (what === "lovelace/config/save") {
        if (m.success) { console.log("dashboard config saved:", `/${URL_PATH}`); ws.close(); resolve(); }
        else { console.error("SAVE FAILED:", m.error); process.exit(6); }
      }
    });
    ws.addEventListener("error", (e) => { console.error("WS ERROR:", e.message || e); process.exit(7); });
    setTimeout(() => { console.error("TIMEOUT"); process.exit(8); }, 15000);
  });
}

(async () => {
  await publishDiscovery();
  await saveDashboard();
  console.log("done.");
})();
