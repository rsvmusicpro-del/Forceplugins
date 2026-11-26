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
if manifest["version"] != "1.9.11":
    raise SystemExit(f"Unexpected manifest version: {manifest['version']}")
if not readme.startswith(f"# StrattPultek {manifest['version']}\n"):
    raise SystemExit("README version heading does not match plugin.json")
if not dsp_notes.startswith(f"# StrattPultek {manifest['version']} DSP notes"):
    raise SystemExit("DSP-NOTES version heading does not match plugin.json")
if ui.get("kit", {}).get("theme") != "amber":
    raise SystemExit("UI theme must remain amber")
if ui.get("kit", {}).get("accent") != "ffd89a48":
    raise SystemExit("UI amber accent changed")
if ui.get("qlinkGroups") != "rows":
    raise SystemExit("Q-Link orientation must remain rows")
if ui.get("style", {}).get("background") != "ff17191c":
    raise SystemExit("UI base background changed")
all_controls = [control for page in ui["pages"] for control in page["controls"] if isinstance(control, dict)]
if any(control.get("widget") not in ("knob", "fader", "switch") for control in all_controls):
    raise SystemExit("UI contains an unsupported or generic control widget")
control_names = [control.get("param") for control in all_controls]
if len(control_names) != len(set(control_names)):
    raise SystemExit("A parameter is displayed more than once in the touch UI")
if len(ui["pages"]) > 6:
    raise SystemExit("Touch UI exceeds the six-page MPC navigation limit")
pages = [page["name"] for page in ui["pages"]]
if pages != ["CONSOLE", "FILTERS", "EQ", "CIRCUIT"]:
    raise SystemExit(f"Unexpected page layout: {pages!r}")
filter_page = next(page for page in ui["pages"] if page["name"] == "FILTERS")
if filter_page["controls"] != [
    {"param": "HP Freq", "widget": "knob"},
    {"param": "LP Freq", "widget": "knob"},
    "empty", "empty",
]:
    raise SystemExit("FILTERS page does not match the shared StrattTrack layout")
console_page = next(page for page in ui["pages"] if page["name"] == "CONSOLE")
if any(isinstance(control, dict) and control.get("param") in ("HP Freq", "LP Freq")
       for control in console_page["controls"]):
    raise SystemExit("HP/LP controls must live only on the FILTERS page")

display_names = set(re.findall(r'addParam\(\w+Param\("[^"]+",\s*"([^"]+)"', source))
referenced = {
    control["param"]
    for page in ui["pages"]
    for control in page["controls"]
    if isinstance(control, dict) and "param" in control
}
if "i.version = 1911;" not in source:
    raise SystemExit("PluginInfo version does not match the 1.9.11 manifest")
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
if '#include "dsp/two_pole_filter.h"' not in source:
    raise SystemExit("Shared second-order Butterworth filter header is missing")
if "TwoPoleButterworth highpass" not in source or "TwoPoleButterworth lowpass" not in source:
    raise SystemExit("Shared second-order Butterworth HP/LP sections are missing")
if "CleansweepOnePole" in source or "dsp/cleansweep_filter.h" in source:
    raise SystemExit("Retired Cleansweep one-pole filters remain in the audio path")
if 'logParam("hp_freq", "HP Freq", 20.0f, 1000.0f, 20.0f' not in source:
    raise SystemExit("HP range does not match StrattTrack")
if 'logParam("lp_freq", "LP Freq", 1000.0f, 22000.0f, 22000.0f' not in source:
    raise SystemExit("LP range does not match StrattTrack")
if "smoothFilterFrequency" not in source or "0.010f * static_cast<float>(sampleRate)" not in source:
    raise SystemExit("Shared 10 ms logarithmic filter glide is missing")
print("StrattPultek parameter identity, presets, MPC Sample mapping and shared 4-page FILTERS contract passed")
