#!/usr/bin/env python3
"""Contract checks for StrattPultek's saved-project and touch-UI compatibility."""
import json
import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
project = root / "plugins" / "StrattPultek"
source = (project / "src" / "StrattPultek.cpp").read_text(encoding="utf-8")
ui = json.loads((project / "ui.json").read_text(encoding="utf-8"))
manifest = json.loads((project / "plugin.json").read_text(encoding="utf-8"))

ids = re.findall(r'addParam\(\w+Param\("([^"]+)"', source)
expected = [
    "output", "low_freq", "low_boost", "low_atten", "high_freq", "high_boost",
    "bandwidth", "hf_att_freq", "hf_atten", "grain", "drive", "mix",
    "drive_mix", "fader", "hp_freq", "lp_freq",
]
if ids != expected:
    raise SystemExit(f"Parameter order/identity mismatch: {ids!r}")
if manifest["version"] != "1.9.0":
    raise SystemExit(f"Unexpected manifest version: {manifest['version']}")
pages = [page["name"] for page in ui["pages"]]
if pages != ["FILTER", "EQ", "CIRCUIT"]:
    raise SystemExit(f"Unexpected page layout: {pages!r}")

display_names = set(re.findall(r'addParam\(\w+Param\("[^"]+",\s*"([^"]+)"', source))
referenced = {
    control["param"]
    for page in ui["pages"]
    for control in page["controls"]
    if isinstance(control, dict) and "param" in control
}
if not referenced.issubset(display_names):
    raise SystemExit(f"UI references undeclared parameters: {sorted(referenced - display_names)}")
programs = re.findall(r'addProgram\("[^"]+",\s*\{([^}]+)\}\)', source)
if not programs or any(len(values.split(",")) != len(expected) for values in programs):
    raise SystemExit("Factory program values do not match the 16-parameter contract")
expected_knobs = {"knobs": [2, 5, 9], "shiftKnobs": [3, 8, 13]}
for mapping, expected_indices in expected_knobs.items():
    actual_indices = [entry["parameter"] for entry in manifest[mapping]]
    if actual_indices != expected_indices:
        raise SystemExit(f"MPC Sample {mapping} mapping changed: {actual_indices!r}")
print("StrattPultek parameter identity, presets, MPC Sample mapping and 3-page UI contract passed")
