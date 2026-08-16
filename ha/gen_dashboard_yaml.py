#!/usr/bin/env python3
"""Generate the YAML-mode Lovelace dashboard from the JSON source of truth.

The HA instance this project targets serves its dashboards in YAML mode. HA
refuses `lovelace/config/save` on those ("Not supported"), so `apply_ha.js`
cannot push the dashboard the way it pushes MQTT discovery -- the file has to
be installed into the HA config directory instead.

Keeping `giessanlage_dashboard.json` as the single source and generating the
YAML avoids the two drifting apart. The round-trip assertion at the end is the
point: it fails loudly rather than emitting a dashboard that silently differs
from the JSON.

Usage (from the repo root):
    python3 ha/gen_dashboard_yaml.py
"""

import json
import pathlib
import sys

try:
    import yaml
except ImportError:
    sys.exit("PyYAML required: pip install pyyaml (or run under WSL)")

HERE = pathlib.Path(__file__).parent
SRC = HERE / "giessanlage_dashboard.json"
DST = HERE / "giessanlage_dashboard.yaml"

HEADER = """\
# Gießanlage Lovelace dashboard (YAML mode).
#
# GENERATED from ha/giessanlage_dashboard.json by ha/gen_dashboard_yaml.py.
# Edit the JSON and regenerate; do not hand-edit this file.
#
# Why this exists: the target HA instance serves dashboards in YAML mode, where
# HA refuses lovelace/config/save ("Not supported"). apply_ha.js can still push
# the MQTT discovery message, but not the dashboard -- install this file into
# the HA config directory and reference it from configuration.yaml:
#
#   lovelace:
#     dashboards:
#       giessanlage-watering:
#         mode: yaml
#         filename: giessanlage_dashboard.yaml
#         title: Gießanlage
#         icon: mdi:watering-can
#         show_in_sidebar: true
#
# Reload afterwards with Developer Tools -> YAML -> "Reload Lovelace", or
# restart HA.

"""


def main() -> None:
    cfg = json.loads(SRC.read_text(encoding="utf-8"))

    body = yaml.safe_dump(
        cfg,
        allow_unicode=True,   # keep "Gießanlage" readable rather than escaped
        sort_keys=False,      # preserve the authored card order
        width=120,
        default_flow_style=False,
    )
    DST.write_text(HEADER + body, encoding="utf-8", newline="\n")

    # The generated YAML must parse back to exactly the JSON structure. A
    # mismatch means the emitter mangled a template string (the card templates
    # are full of braces and quotes), which would ship a broken dashboard.
    back = yaml.safe_load(DST.read_text(encoding="utf-8"))
    if back != cfg:
        sys.exit("ROUND-TRIP MISMATCH: generated YAML does not match the JSON")

    print(f"wrote {DST.relative_to(HERE.parent)} - round-trip verified identical")


if __name__ == "__main__":
    main()
