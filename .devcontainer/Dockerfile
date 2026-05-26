FROM mcr.microsoft.com/devcontainers/base:ubuntu-24.04

# Tooling needed by PlatformIO (Python, build essentials, udev for board rules, git)
RUN apt-get update \
    && export DEBIAN_FRONTEND=noninteractive \
    && apt-get install -y --no-install-recommends \
        python3 \
        python3-pip \
        python3-venv \
        build-essential \
        git \
        udev \
        usbutils \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Install PlatformIO Core into a self-contained venv and expose it on PATH.
ARG PIO_VENV=/opt/platformio
RUN python3 -m venv ${PIO_VENV} \
    && ${PIO_VENV}/bin/pip install --no-cache-dir -U pip platformio
ENV PATH=/opt/platformio/bin:${PATH}

# Let the non-root devcontainer user reach serial devices (dialout/plugdev).
RUN usermod -aG dialout,plugdev vscode || true

# PlatformIO writes downloaded platforms/toolchains/packages to its core dir
# (~/.platformio). Create it owned by the non-root user so the named cache
# volume mounted there initializes as vscode:vscode rather than root:root.
RUN mkdir -p /home/vscode/.platformio \
    && chown -R vscode:vscode /home/vscode/.platformio
