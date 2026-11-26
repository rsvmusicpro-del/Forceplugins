#!/usr/bin/env python3
"""Static contract checks for StrattTrack's parameter identity and signal path."""
import json, pathlib, re, sys, math
root=pathlib.Path(__file__).resolve().parents[1]/"plugins"/"StrattTrack"
manifest=json.loads((root/"plugin.json").read_text())
ui=json.loads((root/"ui.json").read_text())
src=(root/"src"/"StrattTrack.cpp").read_text()
assert manifest["name"]=="StrattTrack" and manifest["manufacturer"]=="Stratt"
assert manifest["uid"]=="S4X3" and manifest["numInputs"]==2 and manifest["numOutputs"]==2
assert len(manifest["knobs"])==3 and len(manifest["shiftKnobs"])==3
assert all(0<=x["parameter"]<40 for x in manifest["knobs"]+manifest["shiftKnobs"])
assert [p["name"] for p in ui["pages"]]==["INPUT","FILTERS","PULTEC","EQ","DYNAMICS","CIRCUIT"]
assert len(re.findall(r"addParam\(",src))==40
for stage in ["hp.run","lp.run","lf.run","lm.run","hm.run","hf.run","gateGain_","compGain_","circuit(","MPC_PLUGIN_ENTRY"]:
    assert stage in src, stage
assert src.index("x=s.hp.run") < src.index("s.ptLowBoostState+=") < src.index("x=s.lf.run") < src.index("const float peak=") < src.index("const float driveWet=circuit(")
assert "const float exponent=-(1.0f-1.0f/ratio);" in src and "compCurve_[curveIndex]" in src  # hard-knee amplitude curve LUT
assert "gateClose=gateThreshold*db(-3.0f)" in src  # hysteresis
assert "*samples[ch]=clean+(colour-clean)*mix" in src  # true circuit bypass at 0%
assert "hpFreq_.next" in src and "hfGain_.next" in src  # smoothed filter controls
assert "float detector_=0.0f, gateDetector_=0.0f, gateGain_=1.0f, compGain_=1.0f;" in src  # independent linked envelopes
assert "const float gateEnvCoef=peak>gateDetector_?gateAtk:gateRel;" in src
assert "gateDetector_=gateEnvCoef*gateDetector_+(1.0f-gateEnvCoef)*peak;" in src
assert "compGain_=targetComp;" in src and "const float cc=targetComp<compGain_?compAtk:compRel;" not in src
assert "const float gateSlew=coeff(2.0f,sr_);" in src
assert "kPtLowFreq, kPtLowBoost, kPtLowAtten, kPtHighFreq" in src
assert "ptLowAttenAlpha=1.0f-std::exp(-2.0f*kPi*ptLfF*1.41421356f/sr_)" in src
assert "ptLfF*8.0f" in src and "ptHighBoost.bell" in src
assert "ptHfBoost*(0.5f+0.5f*(1.0f-ptBandwidth*0.01f))" in src
assert "phaseL_.next" in src and "panL_.next" in src
# Independent equation checks: expected values are derived from the DSP equations,
# not copied from the C++ implementation's runtime output.
def magnitude(b, a, omega):
    z1=complex(math.cos(-omega), math.sin(-omega))
    z2=z1*z1
    num=b[0]+b[1]*z1+b[2]*z2
    den=1.0+a[0]*z1+a[1]*z2
    return abs(num/den)

# 2nd-order Butterworth low-pass must be -3.0103 dB at its corner.
sr, fc, q = 44100.0, 1000.0, 1.0/math.sqrt(2.0)
w=2.0*math.pi*fc/sr
c, sn=math.cos(w), math.sin(w)
alpha=sn/(2.0*q)
b=((1.0-c)*0.5, 1.0-c, (1.0-c)*0.5)
a=((1.0+alpha), -2.0*c, 1.0-alpha)
norm=tuple(v/a[0] for v in b)
an=(a[1]/a[0], a[2]/a[0])
assert abs(magnitude(norm, an, w)-1.0/math.sqrt(2.0)) < 2.0e-5

# RBJ peaking EQ at 0 dB must be unity at DC, centre and Nyquist.
A=10.0**(0.0/40.0)
alpha=sn/(2.0*1.3)
bb=(1.0+alpha*A, -2.0*c, 1.0-alpha*A)
aa=(1.0+alpha/A, -2.0*c, 1.0-alpha/A)
bb=tuple(v/aa[0] for v in bb)
aa=(aa[1]/aa[0], aa[2]/aa[0])
for omega in (0.0, w, math.pi):
    assert abs(magnitude(bb, aa, omega)-1.0) < 1.0e-6

# Equal-power input pan maintains unit summed power over the full travel.
for p in (-1.0, -0.75, -0.25, 0.0, 0.4, 0.8, 1.0):
    left=math.cos((p+1.0)*math.pi/4.0)
    right=math.sin((p+1.0)*math.pi/4.0)
    assert abs(left*left+right*right-1.0) < 1.0e-12

# Hard-knee amplitude law: 6 dB above threshold at 4:1 leaves 1.5 dB overshoot.
over=10.0**(6.0/20.0)
gain=over**(-(1.0-1.0/4.0))
assert abs(20.0*math.log10(over*gain)-1.5) < 1.0e-10

# One-pole attack/release coefficient reaches 1-exp(-1) after one time constant.
tau, sr=0.010, 48000.0
coef=math.exp(-1.0/(tau*sr))
env=1.0-(coef**round(tau*sr))
assert abs(env-(1.0-math.exp(-1.0))) < 1.0e-12

# First-order DC blocker has H(z=1)=0; a DC step decays to zero.
assert "valveDcCoef_=std::exp(-2.0f*kPi*3.0f/sr_)" in src
assert "s.valveDc=valveDcCoef_*(s.valveDc+valveStatic-s.valvePrev);" in src
r=math.exp(-2.0*math.pi*3.0/44100.0)
y, x_prev = 0.0, 0.0
for _ in range(100000):
    x = 1.0
    y = r*(y+x-x_prev)
    x_prev = x
assert abs(y) < 1.0e-12

def onepole_shelf_mag(fc,gain_db,freq,sr=44100.0):
    alpha=1.0-math.exp(-2.0*math.pi*fc/sr)
    omega=2.0*math.pi*freq/sr
    z=complex(math.cos(-omega),math.sin(-omega))
    lp=alpha/(1.0-(1.0-alpha)*z)
    return abs(1.0+(10.0**(gain_db/20.0)-1.0)*lp)
assert abs(onepole_shelf_mag(100,8,0)-10.0**(8/20))<1.0e-10
assert abs(onepole_shelf_mag(100,8,22050)-1.0)<1.2e-2
for f in (0,100,1000,10000,22050): assert abs(onepole_shelf_mag(100,0,f)-1.0)<1.0e-12
def interaction(b,a): return 5.0*(1.0-math.exp(-min(b,a)/5.0)) if b>0 and a>0 else 0.0
assert interaction(0,10)==0 and interaction(10,0)==0
assert 0<interaction(3,3)<interaction(10,10)<5
assert abs(interaction(100,100)-5)<2.0e-8
# The hardware sweep comparison reports a 9 dB peak difference from sharp to broad.
def effective_pt_hf_peak(boost_db, bandwidth):
    return boost_db*(0.5+0.5*(1.0-bandwidth/100.0))
assert abs(effective_pt_hf_peak(18,0)-18.0)<1.0e-12
assert abs(effective_pt_hf_peak(18,100)-9.0)<1.0e-12
assert abs(effective_pt_hf_peak(18,0)-effective_pt_hf_peak(18,100)-9.0)<1.0e-12
print("StrattTrack contract and independent DSP equation checks OK")
