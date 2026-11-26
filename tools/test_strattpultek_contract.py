#!/usr/bin/env python3
"""Contract checks for StrattPultek's saved-project and touch-UI compatibility."""
import json
import re
import struct
from pathlib import Path

root = Path(__file__).resolve().parents[1]
project = root / "plugins" / "StrattPultek"
source = (project / "src" / "StrattPultek.cpp").read_text(encoding="utf-8")
ui = json.loads((project / "ui.json").read_text(encoding="utf-8"))
manifest = json.loads((project / "plugin.json").read_text(encoding="utf-8"))
readme = (project / "README.md").read_text(encoding="utf-8")
dsp_notes = (project / "DSP-NOTES.md").read_text(encoding="utf-8")

ids = re.findall(r'addParam\(\w+Param\("([^"]+)"', source)
expected = [
    "output", "low_freq", "low_boost", "low_atten", "high_freq", "high_boost",
    "bandwidth", "hf_att_freq", "hf_atten", "grain", "drive", "mix",
    "drive_mix", "fader", "hp_freq", "lp_freq", "console_fader",
]
if ids != expected:
    raise SystemExit(f"Parameter order/identity mismatch: {ids!r}")
if manifest["version"] != "1.9.10":
    raise SystemExit(f"Unexpected manifest version: {manifest['version']}")
if not readme.startswith(f"# StrattPultek {manifest['version']}\n"):
    raise SystemExit("README version heading does not match plugin.json")
if not dsp_notes.startswith(f"# StrattPultek {manifest['version']} DSP notes"):
    raise SystemExit("DSP-NOTES version heading does not match plugin.json")
pages = [page["name"] for page in ui["pages"]]
if pages != ["CONSOLE", "EQ", "CIRCUIT"]:
    raise SystemExit(f"Unexpected page layout: {pages!r}")

display_names = set(re.findall(r'addParam\(\w+Param\("[^"]+",\s*"([^"]+)"', source))
referenced = {
    control["param"]
    for page in ui["pages"]
    for control in page["controls"]
    if isinstance(control, dict) and "param" in control
}
if "i.version = 1910;" not in source:
    raise SystemExit("PluginInfo version does not match the 1.9.10 manifest")
if '#include "grain_engine.h"' not in source or "channels_[ch].grainDensity.process(" not in source:
    raise SystemExit("Level-independent Grain continuity detector is missing from the audio path")
if "grainDensity(previousDriveInput, densitySample)" in source:
    raise SystemExit("Legacy mixed-domain Grain density path is still present")
if "calculateDriveMakeupGain" not in source or "driveMakeupTable_" not in source or "* driveMakeupGain(drive)" not in source:
    raise SystemExit("Precomputed Drive fundamental makeup is missing from the audio path")
if "kDriveMakeupSteps = 100" not in source or "for (int i = 0; i <= kDriveMakeupSteps; ++i)" not in source:
    raise SystemExit("Drive makeup must be precomputed outside the audio callback")
if "constexpr int kControlQuantum = 64;" not in source or "smoothEq(blockFrames);" not in source:
    raise SystemExit("64-frame coefficient refresh cadence is missing")
if "channels_[ch].tubeStage(processed, dcPole_)" not in source or "tubeOutput * outGain * finalFader" not in source:
    raise SystemExit("Parameter 0 master gain must remain post-colour and tone-neutral")
if "tubeStage(preFader" in source:
    raise SystemExit("Parameter 0 must not drive the nonlinear valve stage")
if "float readParam(int index, float fallback, float minimum, float maximum) const" not in source:
    raise SystemExit("Finite host-parameter guard is missing")
if re.search(r"\bget(?:Int)?\(k[A-Z]\w*\)", source):
    raise SystemExit("DSP contains an unguarded direct parameter read")
if "mpc::clamp(dry, -kMaxInputMagnitude, kMaxInputMagnitude)" not in source:
    raise SystemExit("Audio input headroom guard is missing")
if "if (std::isfinite(outputSample))" not in source:
    raise SystemExit("Final non-finite output recovery is missing")
if not re.search(r"hasPreviousDriveInput\s*\?", source) or not re.search(r"hasPreviousTubeInput\s*\?", source):
    raise SystemExit("ADAA paths must seed their first sample without a false zero-to-signal interval")
if not referenced.issubset(display_names):
    raise SystemExit(f"UI references undeclared parameters: {sorted(referenced - display_names)}")

# Verify that every MPC skin page uses the packaged, correctly sized background.
for page in ui["pages"]:
    if page.get("background") != "strattpultek-bg.png":
        raise SystemExit(f"Missing page background on {page['name']}")
png = (project / "assets" / "strattpultek-bg.png").read_bytes()
if png[:8] != b"\x89PNG\r\n\x1a\n":
    raise SystemExit("UI background is not a PNG")
if struct.unpack(">II", png[16:24]) != (1280, 628):
    raise SystemExit("UI background must be 1280x628 pixels")
if not (project / "assets" / "strattpultek-bg.svg").is_file():
    raise SystemExit("Editable vector source for the UI background is missing")

programs = re.findall(r'addProgram\("[^"]+",\s*\{([^}]+)\}\)', source)
if not programs or any(len(values.split(",")) != len(expected) for values in programs):
    raise SystemExit("Factory program values do not match the 16-parameter contract")
expected_knobs = {"knobs": [2, 5, 9], "shiftKnobs": [3, 8, 13]}
for mapping, expected_indices in expected_knobs.items():
    actual_indices = [entry["parameter"] for entry in manifest[mapping]]
    if actual_indices != expected_indices:
        raise SystemExit(f"MPC Sample {mapping} mapping changed: {actual_indices!r}")
if "console.process(" not in source or "kConsoleFader" not in source:
    raise SystemExit("Airwindows Console7 input stage is missing")
print("StrattPultek parameter identity, presets, MPC Sample mapping and 3-page UI contract passed")
