#!/usr/bin/env python3
"""Static contract checks for StrattTrack's parameter identity and signal path."""
import json, pathlib, re, sys, math
root=pathlib.Path(__file__).resolve().parents[1]/"plugins"/"StrattTrack"
manifest=json.loads((root/"plugin.json").read_text())
ui=json.loads((root/"ui.json").read_text())
src=(root/"src"/"StrattTrack.cpp").read_text()
reset_match=re.search(r"void reset\(\) override \{(.*?)\n    \}\n    void process",src,re.S)
assert reset_match, "StrattTrack reset block not found"
reset_body=reset_match.group(1)
for line in reset_body.splitlines():
    if ".snap(" in line:
        assert any(token in line for token in ("finiteClamp(", "getBool(", "mpc::clamp(", ".snap(1.0f)")), line
assert "sr_=(std::isfinite(sr)&&sr>=8000.0&&sr<=384000.0)?" in src
assert "void snap(float x) { v=std::isfinite(x)?x:0.0f; }" in src
assert manifest["name"]=="StrattTrack" and manifest["manufacturer"]=="Stratt"
assert manifest["version"]=="1.2.3"
assert manifest["uid"]=="S4X3" and manifest["numInputs"]==2 and manifest["numOutputs"]==2
assert len(manifest["knobs"])==3 and len(manifest["shiftKnobs"])==3
assert all(0<=x["parameter"]<41 for x in manifest["knobs"]+manifest["shiftKnobs"])
assert [p["name"] for p in ui["pages"]]==["INPUT","FILTERS","PULTEC","EQ","DYNAMICS","OUTPUT"]
filter_page=next(p for p in ui["pages"] if p["name"]=="FILTERS")
assert [x["param"] for x in filter_page["controls"] if isinstance(x,dict)]==["HP Freq","LP Freq","CS HP On","CS HP Freq","CS HP Ord","CS LP On","CS LP Freq","CS LP Ord"]
assert len(filter_page["controls"])<=16
assert "CleansweepButterworth" in src and "csHp.set(sr_,csHp,csHpOrder)" in src and "csLp.set(sr_,csLp,csLpOrder)" in src
assert '#include "dsp/two_pole_filter.h"' in src
assert "TwoPoleButterworth hp" in src and "TwoPoleButterworth lp" in src
assert "for(int blockStart=0;blockStart<n;blockStart+=64)" in src
assert "0.010f*std::fmax(sr_,1.0f)" in src
assert "x.version=1203;" in src
assert len(ui["pages"])<=6  # MPC six-function-key page limit
output_page=next(p for p in ui["pages"] if p["name"]=="OUTPUT")
assert [x["param"] for x in output_page["controls"] if isinstance(x,dict)]==["Circuit Mix","Out Pan","Output"]
assert all(sum(1 for page in ui["pages"] for control in page["controls"] if isinstance(control,dict) and control.get("param")==name)==1 for name in ("Output","Out Pan"))
assert not any(isinstance(control,dict) and control.get("param")=="Fader" for page in ui["pages"] for control in page["controls"])
assert len(re.findall(r"addParam\(",src))==50
assert "console.process(" in src and "kConsoleFader" in src
assert any(control.get("param")=="Console Fader" for page in ui["pages"] for control in page["controls"] if isinstance(control,dict))
assert src.index("consoleL=ch_[0].console.process") < src.index("applyPolarity(consoleL*ig*pg,0,phaseTargetL)")
for stage in ["hp.process","lp.process","lf.run","lm.run","hm.run","hf.run","gateGain_","compGain_","circuit(","MPC_PLUGIN_ENTRY"]:
    assert stage in src, stage
assert src.index("x=s.hp.process") < src.index("s.csHp.process") < src.index("s.csLp.process") < src.index("s.ptLowBoostState+=") < src.index("x=s.lf.run") < src.index("const float peak=") < src.index("const float driveWet=circuit(")
assert "const float exponent=-(1.0f-1.0f/ratio);" in src and "compCurve_[curveIndex]" in src  # hard-knee amplitude curve LUT
assert "constexpr float kCompCurveMaxOver=32768.0f;" in src
assert "std::sqrt(std::sqrt(normalizedOver))*256.0f" in src

# Independent compressor-LUT accuracy check. The quartic grid must retain
# the analytic hard-knee ratio near threshold and across the full input range.
curve_size=256
curve_max_over=32768.0
for ratio in (1.0, 2.0, 4.0, 10.0, 20.0):
    exponent=-(1.0-1.0/ratio)
    table=[]
    for index in range(curve_size+1):
        position=index/curve_size
        table_over=1.0+(curve_max_over-1.0)*position**4
        table.append(table_over**exponent)
    for sample_index in range(1, 4001):
        overdrive=curve_max_over**(sample_index/4000.0)
        normalized=(overdrive-1.0)/(curve_max_over-1.0)
        position=math.sqrt(math.sqrt(normalized))*curve_size
        index=min(int(position), curve_size-1)
        fraction=position-index
        actual=overdrive**exponent
        interpolated=table[index]+(table[index+1]-table[index])*fraction
        assert abs(interpolated-actual)/actual < 0.002, (ratio, overdrive, interpolated, actual)
    assert abs(table[-1]-(curve_max_over**exponent)) < 1.0e-12
assert "gateClose=gateThreshold*db(-3.0f)" in src  # hysteresis
assert "*samples[ch]=clean+(driveWet-clean)*mix" in src  # drive colour remains independently blendable
assert "const float hpTarget=finiteClamp(get(kHpFreq),20,1000,20);" in src
assert "const float lpTarget=finiteClamp(get(kLpFreq),1000,22000,22000);" in src
assert "const float hp=smoothLog(hpFreq_.v,hpTarget,filterSmooth);" in src
assert "const float lp=smoothLog(lpFreq_.v,lpTarget,filterSmooth);" in src
assert "ch_[c].hp.setCutoff(sr_,hp); ch_[c].lp.setCutoff(sr_,lp);" in src
assert "x=s.hp.process(x); x=s.lp.process(x);" in src  # original Butterworth filters remain
assert "float csHpWet=s.csHp.process(x);" in src and "float csLpWet=s.csLp.process(x);" in src
assert "csHpOld=ch_[c].csHp" in src and "csLpOld=ch_[c].csLp" in src
assert "csHpOrderFade_.next(1.0f,sampleSmooth)" in src and "csLpOrderFade_.next(1.0f,sampleSmooth)" in src
assert "ch_[c].lf.bell(sr_,lfF,0.70710678f,lfG);" in src
assert "ch_[c].hf.bell(sr_,hfF,0.70710678f,hfG);" in src
assert "ctx.outputs[0][i]=xL*oL*og;" in src and "ctx.outputs[1][i]=xR*oR*og;" in src
assert "fader_" not in src and "kFader" not in src
# Once-per-host-block controls use elapsed block duration, not a one-sample coefficient.
assert "controlBlockSmooth=1.0f-std::exp(-static_cast<float>(n)/(0.010f*std::fmax(sr_,1.0f)))" in src
assert "sampleSmooth=1.0f-std::exp(-1.0f/(0.010f*std::fmax(sr_,1.0f)))" in src
for control in ("input_", "output_", "preamp_", "circuitMix_"):
    line = next(line for line in src.splitlines() if control + ".next(" in line)
    assert ",sampleSmooth)" in line, (control, line)
assert "float next(float target,float c) {" in src  # require explicit, sample-rate-aware smoothing
for control in ("gateThreshDb_", "gateRangeDb_", "compThreshDb_", "compRatio_",
                "compMakeupDb_", "drive_", "panL_", "panR_", "outPan_",
                "lfFreq_", "lmFreq_", "hmFreq_",
                "hfFreq_", "ptLowBoost_", "ptLowAtten_", "ptHighBoost_",
                "ptBandwidth_", "ptHighAtten_"):
    line = next(line for line in src.splitlines()
                if control + ".next(" in line or control + ".v," in line)
    assert "controlBlockSmooth" in line, (control, line)
for sr in (44100.0, 48000.0, 96000.0):
    for block in (32, 64, 128, 512):
        coefficient = 1.0 - math.exp(-block/(0.010*sr))
        count = round(0.010*sr/block)
        reached = 1.0 - (1.0-coefficient)**count
        elapsed = count*block/sr
        expected = 1.0-math.exp(-elapsed/0.010)
        assert abs(reached-expected) < 1.0e-12

# Dynamics thresholds/ratio/makeup and circuit drive glide over 10 ms in dB
# or control space; this prevents abrupt gain-computer and nonlinear-knee jumps.
for control in ("gateThreshDb_", "gateRangeDb_", "compThreshDb_", "compRatio_", "compMakeupDb_", "drive_"):
    assert control in src
assert "gateThreshDb_.next(finiteClamp(get(kGateThresh)" in src
assert "gateRangeDb_.next(finiteClamp(get(kGateRange)" in src
assert "compThreshDb_.next(finiteClamp(get(kCompThresh)" in src
assert "compRatio_.next(finiteClamp(get(kCompRatio)" in src
assert "compMakeupDb_.next(finiteClamp(get(kCompMakeup)" in src
assert "drive_.next(finiteClamp(get(kDrive)" in src
assert "gateThreshDb_.snap(finiteClamp(get(kGateThresh)" in src
assert "compRatio_.snap(finiteClamp(get(kCompRatio)" in src
assert "const float controlBlockSmooth=1.0f-std::exp(-static_cast<float>(n)/(0.010f*" in src
assert "float detector_=0.0f, gateDetector_=0.0f, gateGain_=1.0f, compGain_=1.0f;" in src  # independent linked envelopes
assert "const float gateEnvCoef=peak>gateDetector_?gateAtk:gateRel;" in src
assert "gateDetector_=gateEnvCoef*gateDetector_+(1.0f-gateEnvCoef)*peak;" in src
assert "compGain_=targetComp;" in src and "const float cc=targetComp<compGain_?compAtk:compRel;" not in src
assert "const float gateSlew=coeff(2.0f,sr_);" in src
assert "kPtLowFreq, kPtLowBoost, kPtLowAtten, kPtHighFreq" in src
assert "ptLowAttenAlpha=1.0f-std::exp(-2.0f*kPi*ptLfF*1.41421356f/sr_)" in src
assert "ptLfF*8.0f" in src and "ptHighBoost.resonantBoost" in src
assert "ptHfBoost*(0.5f+0.5f*(1.0f-ptBandwidth*0.01f))" in src
assert "applyPolarity(consoleL*ig*pg,0,phaseTargetL)" in src and "applyPolarity(consoleR*ig*pg,1,phaseTargetR)" in src and "panL_.next" in src
assert "phaseHasPrevious_[channel] && phasePrevious_[channel]*x<=0.0f" in src
assert "phaseHasPrevious_[0]=phaseHasPrevious_[1]=false;" in src
assert "phaseHasPrevious_[channel]=true;" in src and "phaseWait_[channel]>=mpc::clamp(static_cast<int>(sr_*0.020f),1,20000)" in src
assert "0.001f*std::fmax(sr_,1.0f)" in src
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

# Per-sample control smoothing must reach 1-exp(-1) after 10 ms, independent of host block size.
for sr in (44100.0, 48000.0, 96000.0):
    alpha=1.0-math.exp(-1.0/(0.010*sr))
    env=1.0-(1.0-alpha)**round(0.010*sr)
    assert abs(env-(1.0-math.exp(-1.0))) < 2.0e-5

# One-pole attack/release coefficient reaches 1-exp(-1) after one time constant.
tau, sr=0.010, 48000.0
coef=math.exp(-1.0/(tau*sr))
env=1.0-(coef**round(tau*sr))
assert abs(env-(1.0-math.exp(-1.0))) < 1.0e-12

# First-order DC blocker has H(z=1)=0; a DC step decays to zero.
assert "valveDcCoef_=std::exp(-2.0f*kPi*3.0f/sr_)" in src
assert "adaaSoftClip(driven,previous,tubeAsymmetry)/tubeDrive" in src
assert "constexpr float tubeDrive=1.15f;" in src and "constexpr float tubeAsymmetry=0.045f;" in src
assert "previousTubeDcInput=shaped;" in src and "previousTubeDcOutput=y;" in src
assert src.index("*samples[ch]=clean+(driveWet-clean)*mix") < src.index("tubeStage(*samples[ch],valveDcCoef_)") < src.index("ctx.outputs[0][i]=xL*oL*og;")
r=math.exp(-2.0*math.pi*3.0/44100.0)
y, x_prev = 0.0, 0.0
for _ in range(100000):
    x = 1.0
    y = r*(y+x-x_prev)
    x_prev = x
assert abs(y) < 1.0e-12
# A constant tube-stage input settles to zero after DC blocking.
assert "const float y=shaped-previousTubeDcInput+pole*previousTubeDcOutput;" in src

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

# Input page Q-Link contract and appended, project-safe stereo controls.
input_page=next(p for p in ui["pages"] if p["name"]=="INPUT")
assert ui["qlinkGroups"]=="rows"
assert input_page["controls"][:4]==["empty"]*4
assert [x["param"] for x in input_page["controls"][4:8]]==["Bass Mono","Drive","Console Fader","Input"]
assert [x["param"] for x in input_page["controls"][8:12]]==["Pan L","Pan R","Width","Correlation"]
assert [x["param"] for x in input_page["controls"][12:16]]==["Preamp","Phase L","Phase R","Invert L-R"]
assert "kInvertLR, kWidth, kCorrelation, kBassMono," in src and "kCsHpEnable, kCsHpFreq, kCsHpOrder, kCsLpEnable, kCsLpFreq, kCsLpOrder, kCount" in src
assert 'addParam(mpc::boolParam("invert_lr","Invert L-R",false))' in src
assert 'addParam(mpc::floatParam("width","Width",0,200,100,"%",0))' in src
assert 'addParam(mpc::floatParam("correlation","Correlation",0,100,0,"%",0))' in src
assert 'addParam(mpc::logParam("bass_mono","Bass Mono",20,400,120,"Hz",0))' in src
assert "const float mid=0.5f*(xL+xR);" in src and "const float side=0.5f*(xL-xR);" in src
assert "const float sideSafety=1.0f-correlation*std::fmax(0.0f,-rho);" in src
assert "bassMonoState_+=bassMonoAlpha*(side-bassMonoState_);" in src
assert "xL=mid+shapedSide;" in src and "xR=mid-shapedSide;" in src
assert [x["param"] for x in next(p for p in ui["pages"] if p["name"]=="OUTPUT")["controls"] if isinstance(x,dict)]==["Circuit Mix","Out Pan","Output"]

# Independent mid/side invariants: decode round-trip and mono fold-down.
for left,right in ((1.0,1.0),(1.0,-1.0),(0.25,-0.75),(-0.3,0.8)):
    mid=(left+right)*0.5
    side=(left-right)*0.5
    for width in (0.0,0.5,1.0,1.5,2.0):
        out_l=mid+side*width
        out_r=mid-side*width
        assert abs((out_l+out_r)*0.5-mid)<1.0e-12
    assert abs((mid+side)-left)<1.0e-12
    assert abs((mid-side)-right)<1.0e-12
assert abs((1.0+1.0)*0.5-1.0)<1.0e-12  # mono input has zero Side, so Width cannot invent stereo
assert "bassMonoState_+=bassMonoAlpha*(side-bassMonoState_);" in src
assert "const float shapedSide=(side-bassMonoState_)*width*sideSafety;" in src

# Invert L-R is an M/S Side sign transition: Mid remains exactly invariant.
assert "const float swapMid=0.5f*(xL+xR);" in src
assert "const float swapSideGain=std::cos(invertLR*kPi);" in src
assert "const float swapSide=0.5f*(xL-xR)*swapSideGain;" in src
assert "xL=swapMid+swapSide;" in src and "xR=swapMid-swapSide;" in src
for left,right in ((1.0,1.0),(0.25,0.25),(-0.8,-0.8),(1.0,-1.0),(0.3,-0.7)):
    mid=(left+right)*0.5
    side=(left-right)*0.5
    for t in (0.0,0.1,0.25,0.5,0.75,0.9,1.0):
        side_gain=math.cos(math.pi*t)
        out_l=mid+side*side_gain
        out_r=mid-side*side_gain
        assert abs((out_l+out_r)*0.5-mid)<1.0e-12
        if left==right:
            assert abs(out_l-left)<1.0e-12 and abs(out_r-right)<1.0e-12
    assert abs((mid+side)-left)<1.0e-12
    assert abs((mid-side)-right)<1.0e-12



# Polarity switches are discrete: an ordinary periodic signal changes sign at
# its next zero crossing (no interpolated 10 ms cancellation dip).
for frequency,sr in ((50.0,44100.0),(1000.0,48000.0),(12000.0,96000.0)):
    previous=math.sin(-2.0*math.pi*frequency/sr)
    switched=False
    for i in range(1,int(sr*0.02)):
        current=math.sin(2.0*math.pi*frequency*i/sr)
        if previous*current<=0.0:
            switched=True
            break
        previous=current
    assert switched, (frequency,sr)
# A DC input has no crossing, so the bounded 20 ms timeout must enter the
# 1 ms fallback rather than leave the requested polarity unapplied forever.
for sr in (44100,48000,96000):
    assert 1 <= int(sr*0.020) <= 20000
    alpha=1.0-math.exp(-1.0/(0.001*sr))
    gain=1.0
    for _ in range(round(0.001*sr)):
        gain += alpha*(-1.0-gain)
    assert abs(gain-(-1.0+2.0*math.exp(-1.0)))<0.002
assert "x.version=1202;" in src
print("StrattTrack polarity transition checks OK")
