# Dev Container

An Ubuntu-based dev container for building and testing the firmware, wired through
`docker-compose.yml`. It provides PlatformIO Core, Node.js, and the Claude Code CLI,
and runs as the non-root `vscode` user.

## What's inside

- **PlatformIO Core** (`pio`) installed into a venv on `PATH` — build with `pio run`,
  test with `pio test -e native`.
- **Node.js**, **GitHub CLI** (`gh`), and the **Claude Code** CLI, added via dev
  container features.
- An **Eclipse Mosquitto** MQTT broker (the `mqtt` service) for testing.
- VS Code extensions: PlatformIO IDE, C/C++, Python.

## Usage

Open the folder in VS Code and run **Dev Containers: Reopen in Container**, or use the
Dev Containers CLI. The PlatformIO download cache persists across rebuilds in the
`pio-cache` named volume.

## Persistence and credentials

- Claude Code config/state — settings, history, caches, **and credentials** — lives in
  the gitignored, repo-scoped folder `.devcontainer/.claude-home`, bind-mounted at
  `~/.claude`. It persists across rebuilds and does not bleed into other repositories.
- Authenticate **once inside the container** by running `claude` and following the
  prompt; the token is written into `.claude-home` (writable, so OAuth token refresh
  works) and survives rebuilds. The host's `~/.claude` is intentionally **not** mounted:
  a read-only credentials mount blocks token refresh and re-auth from persisting, and a
  read-write one would let the container modify host credentials. To avoid an interactive
  login you can instead set a long-lived `CLAUDE_CODE_OAUTH_TOKEN` (from
  `claude setup-token`) via `containerEnv` or a Codespaces secret.
- Claude Code telemetry, error reporting, and other non-essential traffic are disabled
  via `CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC=1`.

## MQTT broker (testing)

An Eclipse Mosquitto broker runs as the `mqtt` service and starts alongside the dev
container (via `depends_on`).

- **From the dev container:** `mqtt://mqtt:1883` (Compose resolves the service name).
- **From the host:** `localhost:1883`, plus MQTT-over-WebSockets on `9001`.
- **Authentication is required** — anonymous access is disabled. Credentials come from
  `MQTT_USERNAME` / `MQTT_PASSWORD`, seeded into the broker's password file on each start.
  Defaults are `tester` / `testpass`; override them by copying `.env.example` to
  `.devcontainer/.env` (gitignored) and editing the values, then rebuilding.

Quick check from the host (or the container, using host `mqtt`):

```bash
mosquitto_sub -h localhost -p 1883 -u tester -P testpass -t 'test/#' &
mosquitto_pub -h localhost -p 1883 -u tester -P testpass -t 'test/hello' -m 'hi'
```

The config lives in `.devcontainer/mosquitto/mosquitto.conf`; broker data persists in the
`mqtt-data` volume. These settings are for local testing only — do not reuse them in
production.

## USB passthrough (flashing the ESP32)

Serial passthrough is wired into the default container: `devcontainer.json` layers
`docker-compose.usb.yml` on top of `docker-compose.yml`, which maps `/dev/ttyUSB0` (the
CP210x UART bridge) into the container. If your C6 is wired through its **native USB**
port instead, it enumerates as `/dev/ttyACM0` — update the mapping in
`docker-compose.usb.yml`.

A `postStartCommand` in `devcontainer.json` `chgrp dialout` + `chmod g+rw` the device on
start (docker-desktop has no udev to do this), so the non-root `vscode` user can use it.

> **Important:** the device mapping is validated when the container is created, so the
> board must be reachable in the backend **before** you build/reopen the container.
> Prepare it first (see below), otherwise create fails with
> `error gathering device information ... no such file or directory`.

### Docker Desktop on Windows

Two host-side prerequisites must be satisfied before opening the container:

1. The board must be bound into WSL2 with
   [`usbipd-win`](https://github.com/dorssel/usbipd-win).
2. The `cp210x` driver must be loaded in the **docker-desktop** distro (the one the
   Docker daemon reads device paths from). It is **not auto-loaded**, and Docker Desktop
   does not persist the change, so it must be re-loaded after every Docker Desktop / WSL
   restart or replug.

The helper script does both. In an **administrator PowerShell on the host**:

```powershell
winget install usbipd    # once, if not already installed
powershell -ExecutionPolicy Bypass -File .devcontainer\usb-attach.ps1
```

Then build/reopen the dev container. The equivalent manual steps:

```powershell
usbipd list                            # find the BUSID of the CP2102N (10c4:ea60)
usbipd bind   --busid <BUSID>          # once per device (persists)
usbipd attach --wsl --busid <BUSID>    # attach into WSL2
wsl -d docker-desktop modprobe cp210x  # create /dev/ttyUSB0 in the docker daemon's distro
```

Verify inside the container:

```bash
ls -l /dev/ttyUSB0   # present and group-writable; vscode is in the dialout group
```

If you don't have a board attached and just want to build/test, start without the USB
overlay so create doesn't fail:

```bash
docker compose -f .devcontainer/docker-compose.yml up   # no device mapping
```

(or temporarily drop `docker-compose.usb.yml` from `dockerComposeFile`).

### Linux host

`usbipd` and the `modprobe` step are not needed — the device is present directly once
the board is plugged in (typically with a working udev rule for the `dialout` group).
Adjust the mapping in `docker-compose.usb.yml` if the board enumerates under a different
path.

### Note on the upload port

`platformio.ini` sets `upload_port = COM7`, which is a Windows host port and does not
resolve inside the Linux container. To flash **from the container**, target
`/dev/ttyUSB0` (e.g. `pio run -t upload --upload-port /dev/ttyUSB0`) or remove the
`upload_port` line so PlatformIO auto-detects the single attached board on both host and
container.
