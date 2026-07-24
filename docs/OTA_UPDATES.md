# LAN OTA Updates

Firmware can be flashed over WiFi instead of a serial cable, using the
ESP32 Arduino core's `ArduinoOTA` library. The listener is wired up in
`src/ota_glue.cpp` (`otaSetup()` / `otaTick()`, called from
`src/main.cpp`) and is **disabled by default** — it only starts if an
OTA password is configured.

## One-time setup

1. Copy `secrets.ini.example` to `secrets.ini` if you haven't already
   (see the main secrets setup for WiFi/MQTT credentials).
2. Set an OTA password:

   ```ini
   [secrets]
   ...
   ota_pass = some-strong-password
   ```

   This becomes the `--auth` password `ArduinoOTA` requires before
   accepting a flash — without it, anyone on the same WiFi network
   could push firmware to the device. Leaving `ota_pass` empty keeps
   the OTA listener off entirely (the device logs `OTA: no password
   configured — OTA listener disabled` on serial and only accepts
   serial flashes).
3. Flash the device once over serial with a build that includes this
   password, so the OTA listener actually starts:

   ```bash
   pio run -t upload
   ```

   Confirm on the serial monitor (`pio device monitor`) that you see
   `OTA: listener started` after boot.

## Flashing over LAN

Once the device is running a build with `ota_pass` set and is joined
to WiFi, use the dedicated PlatformIO environment instead of the
serial one:

```bash
pio run -e esp32-c6-devkitc-1-ota -t upload
```

This targets `giessanlage.local` (the device's mDNS hostname, matching
`WiFi.setHostname("giessanlage")`) and authenticates with the same
`ota_pass` from `secrets.ini` via `upload_flags = --auth=...` in
`platformio.ini`. The build itself is identical to the serial
environment (`extends = env:esp32-c6-devkitc-1`) — only the upload
transport differs.

If `giessanlage.local` doesn't resolve on your network (some routers
or OSes don't do mDNS reliably), override the target with the
device's IP instead:

```bash
pio run -e esp32-c6-devkitc-1-ota -t upload --upload-port 192.168.1.42
```

The default `pio run -t upload` (no `-e`) still targets the serial
environment, so existing serial-flash habits/scripts are unaffected —
LAN OTA is opt-in.

## Changing the OTA password later

Update `ota_pass` in `secrets.ini` and reflash once over whichever
transport still works (serial, or LAN OTA with the *old* password).
`Secrets` syncs the new build-time value into NVS on that boot, and
subsequent OTA uploads must use the new password.

## Troubleshooting

- **`OTA: no password configured — OTA listener disabled`** on serial
  — `ota_pass` was empty in the `secrets.ini` used for that build.
  Set it and reflash once over serial.
- **Upload times out / can't find device** — confirm the device is
  connected to WiFi (serial log shows `WiFi: Connected (<ip>)`), then
  try `--upload-port <ip>` instead of the mDNS hostname.
- **Auth failure during upload** — the `ota_pass` baked into the
  currently-running firmware doesn't match what's in your local
  `secrets.ini`/`platformio.ini` right now. Re-flash over serial with
  matching secrets first if they've drifted.

### `No response from device` after `Authenticating ... OK` (Windows Firewall)

This is the most common Windows snag, and it's misleading — the upload
gets *past* auth, then stalls:

```
Authenticating (PBKDF2-HMAC-SHA256)...
OK
Waiting for device...
No response from device
```

The device and password are fine. espota's handshake/auth runs over
**UDP** (which Windows Firewall allows as stateful return traffic), but
the actual firmware image needs the **device to open a fresh, unsolicited
inbound TCP connection back to your PC**. Windows Firewall blocks that by
default — so auth succeeds and the transfer never starts.

The trap: the uploader runs as `…\.platformio\python3\python.exe`. If a
non-interactive run (IDE task, CI, background) ever got silently denied,
Windows may have **auto-created inbound _block_ rules** for that
executable. Block rules override allow rules, so just adding an allow
isn't enough — the block has to go first.

Fix (elevated / "Run as administrator" PowerShell):

```powershell
# 1) Disable any auto-created inbound BLOCK rules for the espota python.
Get-NetFirewallRule -Direction Inbound -Action Block -Enabled True |
  Where-Object { ($_ | Get-NetFirewallApplicationFilter).Program -match 'platformio.*python' } |
  Disable-NetFirewallRule

# 2) Allow inbound from the device's IP (any program/port). Scope it to the
#    device so you're not opening the host up broadly.
New-NetFirewallRule -DisplayName "Giessanlage OTA device" -Direction Inbound `
  -Action Allow -RemoteAddress 192.168.50.12 -Profile Any
```

Then re-run the upload. Notes:

- **Easy path if no block rule exists yet:** run the upload in a normal
  (interactive) terminal and click **Allow access** on the Windows
  Firewall popup when it appears — background/IDE runs can't show that
  prompt, which is exactly how the silent block rules get created.
- **Multi-homed hosts** (WSL, Docker, Hyper-V add extra adapters like
  `172.x` / `192.168.x`): espota advertises the host IP via the UDP
  invitation's source address, which the OS routes over the adapter that
  reaches the device — so this is usually *not* the cause if auth already
  succeeded. If auth itself times out, check that the device's subnet
  adapter is the one being used.
- The device keeps running its current firmware if OTA fails — the new
  image is only booted after a fully received, verified transfer, so a
  failed/blocked OTA is non-destructive.
