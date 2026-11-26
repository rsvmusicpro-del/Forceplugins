#!/usr/bin/env python3
"""Static contract checks for StrattTrack's parameter identity and signal path."""
import json, pathlib, re, sys
root=pathlib.Path(__file__).resolve().parents[1]/"plugins"/"StrattTrack"
manifest=json.loads((root/"plugin.json").read_text())
ui=json.loads((root/"ui.json").read_text())
src=(root/"src"/"StrattTrack.cpp").read_text()
assert manifest["name"]=="StrattTrack" and manifest["manufacturer"]=="Stratt"
assert manifest["uid"]=="S4X3" and manifest["numInputs"]==2 and manifest["numOutputs"]==2
assert len(manifest["knobs"])==3 and len(manifest["shiftKnobs"])==3
assert all(0<=x["parameter"]<32 for x in manifest["knobs"]+manifest["shiftKnobs"])
assert [p["name"] for p in ui["pages"]]==["INPUT","EQ","DYNAMICS","CIRCUIT"]
assert len(re.findall(r"addParam\(",src))==32
for stage in ["hp.run","lp.run","lf.run","lm.run","hm.run","hf.run","gateGain_","compGain_","circuit(","MPC_PLUGIN_ENTRY"]:
    assert stage in src, stage
assert src.index("x=s.hp.run") < src.index("x=s.lf.run") < src.index("const float peak=") < src.index("const float driveWet=circuit(")
assert "const float exponent=-(1.0f-1.0f/ratio);" in src and "compCurve_[curveIndex]" in src  # hard-knee amplitude curve LUT\nassert "gateClose=gateThreshold*db(-3.0f)" in src  # hysteresis\nassert "*samples[ch]=clean+(colour-clean)*mix" in src  # true circuit bypass at 0%\nassert "hpFreq_.next" in src and "hfGain_.next" in src  # smoothed filter controls\nassert "float detector_=0.0f, gateGain_=1.0f, compGain_=1.0f;" in src  # linked state owned outside channels\nprint("StrattTrack manifest, parameter count, UI, DSP isolation, smoothing and Q-Link contract OK")
