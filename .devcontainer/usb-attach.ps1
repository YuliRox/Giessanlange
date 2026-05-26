# Host-side helper (Windows + Docker Desktop) to make the ESP32 reachable inside the
# dev container. Run this in an ADMINISTRATOR PowerShell BEFORE building/reopening the
# container, because the container's device mapping is validated at create time.
#
#   1. Binds + attaches the CP2102N (VID:PID 10c4:ea60) into WSL2 via usbipd-win.
#   2. Loads the cp210x driver in the docker-desktop distro (where the Docker daemon
#      reads device paths) so /dev/ttyUSB0 is created.
#
# Usage:  powershell -ExecutionPolicy Bypass -File .devcontainer\usb-attach.ps1
#
# Re-run after a Windows/Docker Desktop restart or after replugging the board.

$ErrorActionPreference = 'Stop'
$VidPid = '10c4:ea60'   # Silicon Labs CP2102N UART bridge on the ESP32-C6-DevKitC-1

if (-not (Get-Command usbipd -ErrorAction SilentlyContinue)) {
    Write-Error "usbipd-win is not installed. Install it with: winget install usbipd"
}

# Find the BUSID for the UART bridge.
$line = (usbipd list) -split "`r?`n" | Where-Object { $_ -match $VidPid } | Select-Object -First 1
if (-not $line) {
    Write-Error "No device matching $VidPid found. Is the ESP32 plugged in? Check 'usbipd list'."
}
$busid = ($line -split '\s+')[0]
Write-Host "Found UART bridge $VidPid at BUSID $busid"

# Bind (one-time, persists) and attach into WSL2.
if ($line -notmatch 'Attached') {
    usbipd bind --busid $busid 2>$null   # no-op if already bound
    usbipd attach --wsl --busid $busid
    Write-Host "Attached $busid into WSL2."
} else {
    Write-Host "$busid is already attached."
}

# Load the serial driver in the docker-desktop distro so /dev/ttyUSB0 appears.
wsl -d docker-desktop modprobe cp210x
$node = wsl -d docker-desktop sh -c "ls -l /dev/ttyUSB0 2>&1"
Write-Host "docker-desktop: $node"
Write-Host "Done. You can now build/reopen the dev container."
