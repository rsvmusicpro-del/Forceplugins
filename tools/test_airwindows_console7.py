#!/usr/bin/env python3
"""Numerical and integration regression checks for the Airwindows Console7 adaptation."""
import json
import math
import pathlib
import re

root = pathlib.Path(__file__).resolve().parents[1]
header = (root / "framework/dsp/airwindows_console7.h").read_text()
assert "Console7Channel" in header
assert "20000.0" in header and "1.6180339887498948482" in header
assert "1.272019649514069" in header and "1.097" in header
assert "static double primitive" in header and "static double adaa" in header
assert "Airwindows source is MIT licensed" in header

# Compare the degree-21 approximation to the Airwindows Console7 transfer.
def ref(x):
    return 0.8 * math.sin(x * abs(x)) / abs(x) + 0.2 * math.sin(x) if x else 0.0
c = [1.0, -1/30, -0.131666666666666667, -0.0000396825396825396825,
     0.006667217813051146, -0.000000005010421677088344,
     -0.00015873012661207105, -0.0000000000001529432746363963,
     0.000002204585538481163, 0.0, -0.000000020041686708353376]
def poly(x):
    z=x*x
    p=c[-1]
    for a in reversed(c[:-1]): p=a+z*p
    return x*p
for i in range(2001):
    x=-1.097+2.194*i/2000
    assert abs(poly(x)-ref(x)) < 1.6e-7

def primitive(x):
    a=abs(x); lim=1.097; z=min(a,lim)**2
    p=c[-1]/22.0
    for i in range(9,-1,-1): p=c[i]/(2*i+2)+z*p
    value=z*p
    return value if a<=lim else value+poly(lim)*(a-lim)

# ADAA average equals a high-resolution numerical integral over random intervals.
for i in range(1000):
    x=-1.097+2.194*((i*0.61803398875)%1.0)
    p=-1.097+2.194*((i*0.41421356237)%1.0)
    if abs(x-p)<1e-7: continue
    actual=(primitive(x)-primitive(p))/(x-p)
    lo,hi=sorted((x,p)); n=1000; h=(hi-lo)/n
    total=poly(lo)+poly(hi)
    for j in range(1,n): total+=(4 if j&1 else 2)*poly(lo+j*h)
    expected=total*h/(3*(hi-lo))
    assert abs(actual-expected)<2.0e-7

checked = 0
for branch, plugin, ui_name, param_count, ver in [
    ("feature/stratttrack", "StrattTrack", "INPUT", 50, "1.2.7"),
    ("feature/strattpultek", "StrattPultek", "CONSOLE", 17, "1.9.11"),
]:
    project=root/"plugins"/plugin
    if not project.exists():
        continue
    source=(project/"src"/(plugin+".cpp")).read_text()
    manifest=json.loads((project/"plugin.json").read_text())
    ui=json.loads((project/"ui.json").read_text())
    if "console_fader" not in source:
        continue
    checked += 1
    assert len(re.findall(r"addParam\(",source))==param_count
    assert manifest["version"]==ver
    assert ui["pages"][0]["name"]==ui_name
    assert "Console Fader" in source
    assert "console.process(" in source
    assert "consoleFader" in source
    assert "dsp/airwindows_console7.h" in source
    assert "Console7" in manifest["description"]
assert checked == 1
print("Airwindows Console7 transfer, ADAA primitive and branch plugin integration OK")
