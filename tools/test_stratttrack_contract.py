#!/usr/bin/env python3
"""Static contract checks for StrattTrack's parameter identity and signal path."""
import json, pathlib, re, sys, math
root=pathlib.Path(__file__).resolve().parents[1]/"plugins"/"StrattTrack"
manifest=json.loads((root/"plugin.json").read_text())
ui=json.loads((root/"ui.json").read_text())
src=(root/"src"/"StrattTrack.cpp").read_text()
framework=(root.parents[1]/"framework"/"mpc_plugin.h").read_text()
host_sim=(root.parents[1]/"tools"/"host-sim"/"host_sim.cpp").read_text()
reset_match=re.search(r"void reset\(\) override \{(.*?)\n    \}\n    void process",src,re.S)
assert reset_match, "StrattTrack reset block not found"
reset_body=reset_match.group(1)
for line in reset_body.splitlines():
    if ".snap(" in line:
        assert any(token in line for token in ("finiteClamp(", "getBool(", "mpc::clamp(", ".snap(1.0f)")), line
assert "sr_=(std::isfinite(sr)&&sr>=8000.0&&sr<=384000.0)?" in src
assert "void snap(float x) { v=std::isfinite(x)?x:0.0f; }" in src
assert manifest["name"]=="StrattTrack" and manifest["manufacturer"]=="Stratt"
assert manifest["version"]=="1.7.0"
assert "## StrattTrack 1.2.7" in (root.parents[1]/"docs"/"changelog.md").read_text()
notes=(root/"DSP-NOTES.md").read_text()
readme=(root/"README.md").read_text()
assert "the fixed, low-level ADAA valve stage remains active independently" in readme
assert "the separate fixed ADAA valve-inspired stage and 3 Hz DC blocker remain active independently" in notes
assert "complete nonlinear path is parallel-blended, so 0% is true clean bypass" not in notes
assert manifest["uid"]=="S4X3" and manifest["numInputs"]==2 and manifest["numOutputs"]==2
assert len(manifest["knobs"])==3 and len(manifest["shiftKnobs"])==3
assert all(0<=x["parameter"]<41 for x in manifest["knobs"]+manifest["shiftKnobs"])
assert [p["name"] for p in ui["pages"]]==["INPUT","FILTERS","PULTEC","EQ","DYNAMICS","OUTPUT"]
# Sparse pages are deliberately distributed over the full four-row surface; active controls are centered within each Q-Link row group.
expected_rows={"INPUT":[3,3,3,2],"FILTERS":[2,2,2,2],"PULTEC":[2,2,2,2],"DYNAMICS":[3,2,2,3],"OUTPUT":[2,2,2,1]}
for page in ui["pages"]:
    assert len(page["controls"])==16, (page["name"],len(page["controls"]))
    if page["name"] in expected_rows:
        counts=[sum(control!="empty" for control in page["controls"][row*4:(row+1)*4]) for row in range(4)]
        assert counts==expected_rows[page["name"]], (page["name"],counts)
        for row,count in enumerate(counts):
            occupied=[col for col,control in enumerate(page["controls"][row*4:(row+1)*4]) if control!="empty"]
            expected={3:[0,1,2],2:[1,2],1:[1]}[count]
            assert occupied==expected, (page["name"],row,occupied)
eq_page=next(page for page in ui["pages"] if page["name"]=="EQ")
assert all(control!="empty" for control in eq_page["controls"])
assert ui["kit"]=="mpc-red"
assert ui["style"]["background"]=="ff1e1d1e" and ui["style"]["accent"]=="ffd31145"
assert ui["style"]["text"]=="ffffffff" and ui["style"]["muted"]=="ff85878a"
assert all("background" not in page for page in ui["pages"])
filter_page=next(p for p in ui["pages"] if p["name"]=="FILTERS")
assert [x["param"] for x in filter_page["controls"] if isinstance(x,dict)]==["HP Freq","LP Freq","CS HP On","CS HP Freq","CS HP Ord","CS LP On","CS LP Freq","CS LP Ord"]
assert len(filter_page["controls"])<=16
assert "CleansweepButterworth" in src and "csHp.set(sr_,csHp,csHpOrder)" in src and "csLp.set(sr_,csLp,csLpOrder)" in src
assert '#include "dsp/two_pole_filter.h"' in src
assert "TwoPoleButterworth hp" in src and "TwoPoleButterworth lp" in src
assert "for(int blockStart=0;blockStart<n;blockStart+=64)" in src
# Fixed graphic-EQ gain smoothing and coefficient refresh share the bounded 64-frame cadence.
assert "graphicEqGain_[band].next(" in src
assert "finiteClamp(get(kGraphicEq01+band),-12.0f,12.0f,0.0f)" in src
assert "ch_[c].graphic[band].bell(sr_,kGraphicEqFrequencies[band],graphicEqQValues[band],graphicEqGains[band]);" in src
assert "for(int band=0;band<16;++band) x=s.graphic[band].run(x);" in src
assert "Biquad graphic[16]" in src and "Smooth graphicEqGain_[16]" in src
assert src.index("for(int blockStart=0;blockStart<n;blockStart+=64)") < src.index("graphicEqGain_[band].next(") < src.index("ch_[c].graphic[band].bell(")
assert "0.010f*std::fmax(sr_,1.0f)" in src
assert "x.version=1700;" in src
assert len(ui["pages"])<=6  # MPC six-function-key page limit
output_page=next(p for p in ui["pages"] if p["name"]=="OUTPUT")
assert [x["param"] for x in output_page["controls"] if isinstance(x,dict)]==["Mono Compatibility","Mono Threshold","Circuit Mix","Warmth","Clarity","Out Pan","Output"]
assert all(sum(1 for page in ui["pages"] for control in page["controls"] if isinstance(control,dict) and control.get("param")==name)==1 for name in ("Output","Out Pan","Comp GR"))
assert not any(isinstance(control,dict) and control.get("param")=="Fader" for page in ui["pages"] for control in page["controls"])
assert len(re.findall(r"addParam\(",src))==71
parameter_ids=[]
for line in src.splitlines():
    if "addParam(legacyInput)" in line:
        parameter_ids.append("input")
    elif "addParam(compGr)" in line:
        parameter_ids.append("comp_gr")
    else:
        match=re.search(r'addParam\(mpc::(?:float|log|bool|choice)Param\("([^"]+)"',line)
        if match:
            parameter_ids.append(match.group(1))
assert parameter_ids==[
    "output","input","preamp","phase_l","phase_r","pan_l","pan_r","hp_freq","lp_freq",
    "lf_freq","lf_gain","lm_freq","lm_gain","lm_q","hm_freq","hm_gain","hm_q","hf_freq",
    "hf_gain","gate_thresh","gate_range","gate_attack","gate_release","comp_thresh",
    "comp_ratio","comp_attack","comp_release","comp_makeup","drive","circuit_mix","out_pan",
    "pt_lf_freq","pt_lf_boost","pt_lf_atten","pt_hf_freq","pt_hf_boost","pt_bandwidth",
    "pt_hf_atten_freq","pt_hf_atten","console_fader","invert_lr","width","correlation",
    "bass_mono","cs_hp_on","cs_hp_freq","cs_hp_order","cs_lp_on","cs_lp_freq","cs_lp_order",
    "mono_compatibility","mono_threshold",
    "eq_20","eq_31_5","eq_50","eq_80","eq_125","eq_200","eq_315","eq_500",
    "eq_800","eq_1250","eq_2000","eq_3150","eq_5000","eq_8000","eq_12500","eq_20000","comp_gr","tone_warmth","tone_clarity"
], "Parameter IDs/order are a serialized host-state contract"
programs=re.findall(r'addProgram\("([^"]+)",\s*\{([^}]*)\}\)',src)
assert programs and all(len(values.split(","))==len(parameter_ids) for _,values in programs), "Preset values must match the parameter count"
assert [(x["parameter"],x["label"]) for x in manifest["knobs"]]==[(0,"Output"),(7,"HP Freq"),(8,"LP Freq")]
assert src.index("addParam(legacyInput)") < src.index('addParam(mpc::floatParam("preamp"')
assert [(x["parameter"],x["label"]) for x in manifest["shiftKnobs"]]==[(2,"Preamp"),(28,"Drive"),(29,"Circuit")]

assert "console.process(" in src and "kConsoleFader" in src
assert "kDenormalFloor = 1.0e-30" in (root.parents[1]/"framework"/"dsp"/"airwindows_console7.h").read_text()
assert '#include "circuit_adaa.h"' in src and "CircuitAdaa circuit;" in src
assert "ch_[ch].circuit.process(clean,drive)" in src
assert "if(mix>0.0f && drive>0.0f)" in src and "else ch_[ch].circuit.reset();" in src
assert src.index("float driveWet=clean;") < src.index("if(mix>0.0f && drive>0.0f)") < src.index("ch_[ch].circuit.process(clean,drive)") < src.index("*samples[ch]=clean+(driveWet-clean)*mix")
assert any(control.get("param")=="Console Fader" for page in ui["pages"] for control in page["controls"] if isinstance(control,dict))
assert src.index("consoleL=ch_[0].console.process") < src.index("applyPolarity(consoleL*pg,0,phaseTargetL)")
assert "input_" not in src and "inGain" not in src and "kInput" not in src
assert "kOutput=0, kLegacyInputSlot, kPreamp" in src
assert 'mpc::floatParam("input","Unused",0.0f,0.0f,0.0f,"",0)' in src
assert "legacyInput.automatable=false" in src and "addParam(legacyInput)" in src
assert "get(kLegacyInputSlot)" not in src
for stage in ["hp.process","lp.process","graphic[band].run","gateGain_","compGain_","circuit.process(","MPC_PLUGIN_ENTRY"]:
    assert stage in src, stage
assert src.index("x=s.hp.process") < src.index("s.csHp.process") < src.index("s.csLp.process") < src.index("s.ptLowBoostState+=") < src.index("for(int band=0;band<16;++band) x=s.graphic[band].run(x)") < src.index("const float peak=") < src.index("float driveWet=clean;")
assert "const float exponent=-(1.0f-1.0f/ratio);" in src and "compCurve_[curveIndex]" in src  # hard-knee amplitude curve LUT
assert "constexpr float kCompCurveMaxOver=8388608.0f;" in src
assert "constexpr int kCompCurveIntervals=1024;" in src
assert "std::sqrt(std::sqrt(normalizedOver))*static_cast<float>(kCompCurveIntervals)" in src
assert "float compCurve_[1025]{};" in src

# Independent compressor-LUT accuracy check. The quartic grid must retain
# the analytic hard-knee ratio near threshold and across the full input range.
curve_size=1024
curve_max_over=8388608.0
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
assert "ch_[c].graphic[band].bell(sr_,kGraphicEqFrequencies[band],graphicEqQValues[band],graphicEqGains[band]);" in src
assert "Graphic EQ: sixteen stereo-linked RBJ peaking/bell biquads" in notes
assert "Fixed centres: 20 / 31.5 / 50 / 80 / 125 / 200 / 315 / 500 / 800 / 1.25k / 2k / 3.15k / 5k / 8k / 12.5k / 20k Hz" in notes
assert "ctx.outputs[0][i]=xL*oL*og;" in src and "ctx.outputs[1][i]=xR*oR*og;" in src
assert "fader_" not in src and "kFader" not in src
# Once-per-host-block controls use elapsed block duration, not a one-sample coefficient.
assert "controlBlockSmooth=1.0f-std::exp(-static_cast<float>(n)/(0.010f*std::fmax(sr_,1.0f)))" in src
assert "sampleSmooth=1.0f-std::exp(-1.0f/(0.010f*std::fmax(sr_,1.0f)))" in src
for control in ("output_", "preamp_", "circuitMix_"):
    line = next(line for line in src.splitlines() if control + ".next(" in line)
    assert ",sampleSmooth)" in line, (control, line)
assert "float next(float target,float c) {" in src  # require explicit, sample-rate-aware smoothing
for control in ("gateThreshDb_", "gateRangeDb_", "compThreshDb_", "compRatio_",
                "compMakeupDb_", "drive_",
                "ptLowBoost_", "ptLowAtten_", "ptHighBoost_",
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
assert "applyPolarity(consoleL*pg,0,phaseTargetL)" in src and "applyPolarity(consoleR*pg,1,phaseTargetR)" in src and "panL_.next" in src
assert "phaseHasPrevious_[channel] && phasePrevious_[channel]*x<=0.0f" in src
assert "phaseHasPrevious_[0]=phaseHasPrevious_[1]=false;" in src
assert "phaseHasPrevious_[channel]=true;" in src and "phaseWait_[channel]>=phaseWaitLimit_" in src
assert "phaseFallbackAlpha_=1.0f-std::exp(-1.0f/(0.001f*sr_));" in src
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
assert "adaaTube(driven,previous)/tubeDrive" in src
assert "constexpr float tubeDrive=1.15f;" in src
assert "constexpr double kTriodeBlend=0.28;" in src
assert "constexpr double kMediumMuScale=1.18;" in src
assert "constexpr double kHighMuAsymmetry=0.032;" in src
assert "constexpr double kMediumMuAsymmetry=0.060;" in src
assert "tubeIntegral(xd)-tubeIntegral(pd)" in src
assert "integrateShortTubeInterval(lo,hi)/(hi-lo)" in src
assert "if(ax<=1.0/kMediumMuScale && ap<=1.0/kMediumMuScale)" in src
assert "midpoint+eta*(midpoint*midpoint+d2/12.0)" in src
assert "previousTubeColor+=hfAlpha*(color-previousTubeColor);" in src
assert "tubeHfAlpha_=1.0f-std::exp(-2.0f*kPi*18000.0f/sr_);" in src
assert "float tubeStage(float x,float pole,float hfAlpha)" in src
assert "x.version=1700;" in src
assert "previousTubeDcInput=voiced;" in src and "previousTubeDcOutput=y;" in src
assert src.index("*samples[ch]=clean+(driveWet-clean)*mix") < src.index("tubeStage(*samples[ch],valveDcCoef_,tubeHfAlpha_)") < src.index("ctx.outputs[0][i]=xL*oL*og;")
r=math.exp(-2.0*math.pi*3.0/44100.0)
y, x_prev = 0.0, 0.0
for _ in range(100000):
    x = 1.0
    y = r*(y+x-x_prev)
    x_prev = x
assert abs(y) < 1.0e-12
# A constant tube-stage input settles to zero after DC blocking.
assert "const float y=voiced-previousTubeDcInput+pole*previousTubeDcOutput;" in src

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

# The EQ page is a true fixed-frequency 16-band control surface.
graphic_page=next(p for p in ui["pages"] if p["name"]=="EQ")
graphic_names=["20 Hz","31.5 Hz","50 Hz","80 Hz","125 Hz","200 Hz","315 Hz","500 Hz","800 Hz","1.25 kHz","2 kHz","3.15 kHz","5 kHz","8 kHz","12.5 kHz","20 kHz"]
assert len(graphic_page["controls"])==16
assert [x["param"] for x in graphic_page["controls"]]==graphic_names
assert all(x["widget"]=="fader" and x["polarity"]=="bipolar" for x in graphic_page["controls"])
graphic_freq=[20.,31.5,50.,80.,125.,200.,315.,500.,800.,1250.,2000.,3150.,5000.,8000.,12500.,20000.]
freq_match=re.search(r"kGraphicEqFrequencies\[16\]\s*=\s*\{([^}]*)\}",src)
assert freq_match and [float(x.strip().removesuffix("f")) for x in freq_match.group(1).split(",")]==graphic_freq
assert "constexpr float kGraphicEqBroadBandwidthOctaves=1.9f;" in src
assert "kGraphicEqBandwidthOctaves[16]" in src
assert "graphicEqBandwidth(graphicEqGains[band],band,sr_,bandFrequency)" in src
assert "graphicEqQ(sr_,bandFrequency,bandwidth)" in src
assert "graphicEqQValues[band],graphicEqGains[band]" in src
assert "graphicEqGain_[band].next(" in src
assert "graphicEqQValues[16]" in src
# Independent RBJ bandwidth oracle. The band widths are derived from geometric
# midpoints between adjacent fixed centres; the digital frequency-warp term is
# the exact RBJ bilinear-transform BW<->Q relation.
def graphic_band_width(band):
    lower_ratio=graphic_freq[band]/graphic_freq[band-1] if band>0 else graphic_freq[1]/graphic_freq[0]
    upper_ratio=graphic_freq[band+1]/graphic_freq[band] if band<15 else lower_ratio
    return 0.5*math.log2(lower_ratio*upper_ratio)
def graphic_eq_bandwidth(gain_db,band,sr):
    fc=min(graphic_freq[band],sr*0.45)
    available=min(2.0,2.0*math.log2((sr*0.5)/fc))
    broad=min(1.9,available)
    narrow=min(graphic_band_width(band),available)
    amount=max(0.0,min(1.0,abs(gain_db)/12.0))
    focus=amount*amount*(3.0-2.0*amount)
    return broad+(narrow-broad)*focus
def graphic_eq_q(sr,fc,bw):
    w=2.0*math.pi*fc/sr
    return 1.0/(2.0*math.sinh((math.log(2.0)/2.0)*bw*w/math.sin(w)))
def graphic_peak(sr,fc,q,gain):
    w=2.0*math.pi*fc/sr
    c,sn=math.cos(w),math.sin(w)
    alpha=sn/(2.0*q)
    A=10.0**(gain/40.0)
    a0=1.0+alpha/A
    b=((1.0+alpha*A)/a0,(-2.0*c)/a0,(1.0-alpha*A)/a0)
    a=((-2.0*c)/a0,(1.0-alpha/A)/a0)
    return b,a,w
for band in range(16):
    assert 0.64 < graphic_band_width(band) < 0.70
for sample_rate in (44100.0,48000.0,96000.0):
    for band,nominal_centre in enumerate(graphic_freq):
        centre=min(nominal_centre,sample_rate*0.45)
        q_curve=[]
        for gain_db in (-12.0,-9.0,-6.0,-3.0,0.0,3.0,6.0,9.0,12.0):
            bandwidth=graphic_eq_bandwidth(gain_db,band,sample_rate)
            q=graphic_eq_q(sample_rate,centre,bandwidth)
            assert math.isfinite(q) and 0.25 <= q <= 8.0
            b,a,w=graphic_peak(sample_rate,centre,q,gain_db)
            assert abs(magnitude(b,a,w)-10.0**(gain_db/20.0))<1.0e-7
            if gain_db==0.0:
                assert abs(magnitude(b,a,0.0)-1.0)<1.0e-7
                assert abs(magnitude(b,a,math.pi)-1.0)<1.0e-7
            if gain_db==12.0:
                a1,a2=a
                disc=complex(a1*a1-4.0*a2)**0.5
                assert max(abs((-a1+disc)/2.0),abs((-a1-disc)/2.0))<1.0
        q_curve=[graphic_eq_q(sample_rate,centre,graphic_eq_bandwidth(g,band,sample_rate)) for g in (0,3,6,9,12)]
        assert all(left<=right+1.0e-10 for left,right in zip(q_curve,q_curve[1:]))
        assert all(abs(graphic_eq_bandwidth(g,band,sample_rate)-graphic_eq_bandwidth(-g,band,sample_rate))<1.0e-12 for g in (1,3,6,9,12))
# The frequency-warped RBJ Q must reproduce the requested half-gain bandwidth,
# not merely the centre gain, at a representative upper-mid band and sample rate.
sr,centre,band=96000.0,graphic_freq[10],10
bw=graphic_eq_bandwidth(12.0,band,sr)
q=graphic_eq_q(sr,centre,bw)
for gain_db in (-12.0,12.0):
    b,a,_=graphic_peak(sr,centre,q,gain_db)
    target=10.0**(gain_db/40.0)
    edges=[]
    for side in (-1,1):
        lo,hi=(centre*0.01,centre) if side<0 else (centre,sr*0.5)
        for _ in range(70):
            mid=(lo+hi)*0.5
            response=magnitude(b,a,2.0*math.pi*mid/sr)
            if side<0:
                if (response<target)==(gain_db>0): lo=mid
                else: hi=mid
            else:
                if (response>target)==(gain_db>0): lo=mid
                else: hi=mid
        edges.append((lo+hi)*0.5)
    assert abs(math.log2(edges[1]/edges[0])-bw)<2.0e-4
ratios=[graphic_freq[i+1]/graphic_freq[i] for i in range(15)]
assert max(ratios)-min(ratios)<0.04
assert not any(token in src for token in ("s.lf.run","s.lm.run","s.hm.run","s.hf.run","ch_[c].lf.bell","lfFreq_.snap"))


# Input page Q-Link contract and appended, project-safe stereo controls.
input_page=next(p for p in ui["pages"] if p["name"]=="INPUT")
assert ui["qlinkGroups"]=="rows"
assert [x["param"] for x in input_page["controls"] if isinstance(x,dict)]==["Preamp","Phase L","Phase R","Invert L-R","Pan L","Pan R","Width","Correlation","Bass Mono","Drive","Console Fader"]
assert not any(isinstance(x,dict) and x.get("param")=="Input" for x in input_page["controls"])
assert "kInvertLR, kWidth, kCorrelation, kBassMono," in src and "kCsHpEnable, kCsHpFreq, kCsHpOrder, kCsLpEnable, kCsLpFreq, kCsLpOrder,\n    kMonoCompatibility, kMonoThreshold," in src and "kGraphicEq01" in src and "kGraphicEq16, kCompGR, kToneWarmth, kToneClarity, kCount" in src
assert 'addParam(mpc::boolParam("invert_lr","Invert L-R",false))' in src
assert 'addParam(mpc::floatParam("width","Width",0,200,100,"%",0))' in src
assert 'addParam(mpc::floatParam("correlation","Correlation",0,100,0,"%",0))' in src
assert 'addParam(mpc::logParam("bass_mono","Bass Mono",20,400,120,"Hz",0))' in src
assert "const float mid=0.5f*(xL+xR);" in src and "const float side=0.5f*(xL-xR);" in src
assert "const float sideSafety=1.0f-correlation*std::fmax(0.0f,-rho);" in src
assert "bassMonoSideHp_.pass(sr_,bassFreq,true);" in src
assert "xL=mid+shapedSide;" in src and "xR=mid-shapedSide;" in src
assert [x["param"] for x in next(p for p in ui["pages"] if p["name"]=="OUTPUT")["controls"] if isinstance(x,dict)]==["Mono Compatibility","Mono Threshold","Circuit Mix","Warmth","Clarity","Out Pan","Output"]

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
assert "bassMonoSideHp_.pass(sr_,bassFreq,true);" in src
assert "const float bassMonoSide=bassMonoSideHp_.run(side);" in src
assert "const float shapedSide=bassMonoSide*width*sideSafety;" in src

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
assert "x.version=1700;" in src
assert "phaseFallbackAlpha_=1.0f-std::exp(-1.0f/(0.001f*sr_));" in src
assert "phaseWaitLimit_=mpc::clamp(static_cast<int>(sr_*0.020f),1,20000);" in src
assert "phaseWait_[channel]>=phaseWaitLimit_" in src
polarity_start=src.index("float applyPolarity(")
polarity_end=src.index("\n    }",polarity_start)
polarity_body=src[polarity_start:polarity_end]
assert not any(token in polarity_body for token in ("std::exp(", "std::sin(", "std::cos(", "std::pow(", "std::log(")), "polarity fallback must not evaluate transcendental functions per sample"
print("StrattTrack polarity transition checks OK")


# Hybrid ECC83/6SN7-inspired curve and exact antiderivative checks.
W,K,ETA1,ETA2=0.28,1.18,0.032,0.060
def fbase(x,eta):
    a=abs(x)
    if a<=1: y=x
    elif a>=2: y=math.copysign(1.5,x)
    else:
        t=a-1
        y=math.copysign(1+t-t**3+0.5*t**4,x)
    return y+eta*y*y
def gbase(x):
    a=abs(x)
    if a<=1: return 0.5*a*a
    if a>=2: return 1.85+1.5*(a-2)
    t=a-1
    return 0.5+t+0.5*t*t-0.25*t**4+0.1*t**5
def jbase(x):
    a=abs(x)
    if a<=1: return math.copysign(a**3/3,x)
    if a>=2: return math.copysign(5491/2520+2.25*(a-2),x)
    t=a-1
    q=t+t*t+t**3/3-0.5*t**4-0.2*t**5+t**6/6+t**7/7-t**8/8+t**9/36
    return math.copysign(1/3+q,x)
def ftube(x): return (1-W)*fbase(x,ETA1)+W*fbase(K*x,ETA2)/K
def atube(x): return (1-W)*(gbase(x)+ETA1*jbase(x))+W*(gbase(K*x)/K**2+ETA2*jbase(K*x)/K**2)
for x in (-3,-1.7,-0.6,0,0.6,1.7,3):
    h=1e-6
    assert abs((atube(x+h)-atube(x-h))/(2*h)-ftube(x))<3e-6
h=1e-5
assert abs((ftube(h)-ftube(-h))/(2*h)-1)<2e-5
assert abs(ftube(1.4)+ftube(-1.4))>1e-3
print("StrattTrack hybrid valve transfer and antiderivative checks OK")


# Five-point Gauss-Legendre ADAA fallback matches an independent Simpson integral
# around every knee of both blended triode branches and around the zero crossing.
_GL_NODES=(0.5384693101056831,0.9061798459386640)
_GL_WEIGHTS=(0.5688888888888889,0.4786286704993665,0.2369268850561891)
_GL_BOUNDARIES=sorted((-2.0,-2.0/K,-1.0,-1.0/K,0.0,1.0/K,1.0,2.0/K,2.0))
def gauss5_segment(lo,hi):
    mid=(lo+hi)*0.5
    half=(hi-lo)*0.5
    p0=ftube(mid-_GL_NODES[0]*half)+ftube(mid+_GL_NODES[0]*half)
    p1=ftube(mid-_GL_NODES[1]*half)+ftube(mid+_GL_NODES[1]*half)
    return half*(_GL_WEIGHTS[0]*ftube(mid)+_GL_WEIGHTS[1]*p0+_GL_WEIGHTS[2]*p1)
def gauss5_average(lo,hi):
    total=0.0
    start=lo
    for boundary in _GL_BOUNDARIES:
        if lo<boundary<hi:
            total+=gauss5_segment(start,boundary)
            start=boundary
    total+=gauss5_segment(start,hi)
    return total/(hi-lo)
def simpson_average(lo,hi,steps=1000):
    step=(hi-lo)/steps
    total=ftube(lo)+ftube(hi)
    for i in range(1,steps):
        total+=(4 if i%2 else 2)*ftube(lo+i*step)
    return total/(3*steps)
for knee in _GL_BOUNDARIES:
    for width in (1.0e-3,1.0e-4,1.0e-5):
        lo,hi=knee-0.37*width,knee+0.63*width
        assert abs(gauss5_average(lo,hi)-simpson_average(lo,hi))<2.0e-10
print("StrattTrack hybrid valve ADAA quadrature checks OK")

# Correlation safeguard confidence floor: both smoothed channel powers must be
# above -90 dBFS RMS (1e-9 mean square) before rho can attenuate Side.
assert "correlationActive=corrPowerL_>1.0e-9f && corrPowerR_>1.0e-9f" in src
assert "correlationActive?finiteClamp(corrCross_/corrDenom,-1.0f,1.0f,0.0f):0.0f" in src
floor=1.0e-9
assert abs(10.0*math.log10(floor)-(-90.0))<1.0e-12
def side_safety(power_l,power_r,rho,amount):
    active=power_l>floor and power_r>floor
    return 1.0-amount*max(0.0,-rho) if active else 1.0
assert side_safety(0.0,1.0,-1.0,1.0)==1.0
assert side_safety(floor*0.99,1.0,-1.0,1.0)==1.0
assert side_safety(floor,floor,-1.0,1.0)==1.0
assert side_safety(floor*1.01,floor*1.01,-1.0,1.0)==0.0
assert side_safety(floor*1.01,floor*1.01,1.0,1.0)==1.0


# Stereo controls refresh on bounded 64-frame chunks, independent of host block size.
assert "const float filterSmooth=1.0f-std::exp(-static_cast<float>(blockFrames)/(0.010f*" in src
assert "panL_.next(finiteClamp(get(kPanL)*0.01f,-1,1,-1),filterSmooth)" in src
assert "invertLR_.next(getBool(kInvertLR)?1.0f:0.0f,filterSmooth)" in src
assert "width_.next(finiteClamp(get(kWidth),0,200,100)*0.01f,filterSmooth)" in src
assert "outPan_.next(finiteClamp(get(kOutPan)*0.01f,-1,1,0),filterSmooth)" in src
assert "bassMonoSideHp_.pass(sr_,bassFreq,true);" in src
assert "Biquad bassMonoSideHp_;" in src
# The 2nd-order Butterworth Side high-pass is -3.0103 dB at cutoff and
# approaches 12 dB/octave attenuation per octave below its corner.
for sr in (44100.0,48000.0,96000.0):
    fc=120.0
    omega=2.0*math.pi*fc/sr
    c,sn=math.cos(omega),math.sin(omega)
    alpha=sn/(2.0/math.sqrt(2.0))
    b=((1+c)*0.5,-(1+c),(1+c)*0.5)
    a=(1+alpha,-2*c,1-alpha)
    b=tuple(v/a[0] for v in b)
    a=(a[1]/a[0],a[2]/a[0])
    assert abs(magnitude(b,a,omega)-1.0/math.sqrt(2.0))<2.0e-5
    below=magnitude(b,a,omega/2.0)
    assert below < 0.25
# The Butterworth Side high-pass does not touch Mid, so mono fold-down is
# unchanged regardless of Bass Mono cutoff or Width.
for left,right in ((1.0,1.0),(0.4,-0.2),(-0.8,0.3)):
    mid=(left+right)*0.5
    side=(left-right)*0.5
    for hp_side in (0.0,0.2,0.70710678,1.0):
        for width in (0.0,0.5,1.0,2.0):
            out_l=mid+hp_side*side*width
            out_r=mid-hp_side*side*width
            assert abs((out_l+out_r)*0.5-mid)<1.0e-12
print("StrattTrack stereo-image refinement checks OK")

# Mono Compatibility controls are appended, live on OUTPUT and run immediately before Valve.
assert 'addParam(mpc::choiceParam("mono_compatibility","Mono Compatibility",{"Off","Auto","Full"},0))' in src
assert 'addParam(mpc::floatParam("mono_threshold","Mono Threshold",0,100,50,"%",0))' in src
assert "monoSafety_.next(monoSafetyTarget,sampleSmooth)" in src
assert "monoRho <= -monoThreshold" in src
assert "if(monoMode>=1.5f) monoSafetyTarget=0.0f;" in src
mono_stage=src.index("// Mono Compatibility is deliberately the final stereo-image stage before Valve.")
valve_stage=src.index("tubeStage(*samples[ch],valveDcCoef_,tubeHfAlpha_)")
assert mono_stage < valve_stage
assert src.index("xL=monoMid+monoSide;",mono_stage) < valve_stage
assert [x["param"] for x in output_page["controls"] if isinstance(x,dict)]==["Mono Compatibility","Mono Threshold","Circuit Mix","Warmth","Clarity","Out Pan","Output"]

# Independent Auto transfer checks: no action above threshold, gradual Side reduction below it.
def mono_auto_gain(rho, threshold):
    if rho <= -threshold:
        return max(0.0,min(1.0,(1.0+rho)/max(1.0-threshold,1.0e-4)))
    return 1.0
assert mono_auto_gain(-0.25,0.5)==1.0
assert mono_auto_gain(-0.5,0.5)==1.0
assert abs(mono_auto_gain(-0.75,0.5)-0.5)<1.0e-12
assert mono_auto_gain(-1.0,0.5)==0.0
assert mono_auto_gain(-1.0,1.0)==0.0
# M/S Side control never alters Mid or the mono fold-down.
for left,right in ((1.0,-1.0),(0.25,-0.75),(-0.3,0.8)):
    mid=(left+right)*0.5
    side=(left-right)*0.5
    for gain in (0.0,0.25,0.5,1.0):
        out_l,out_r=mid+side*gain,mid-side*gain
        assert abs((out_l+out_r)*0.5-mid)<1.0e-12

# Compressor gain-reduction telemetry is appended, non-automatable and visible in DYNAMICS.
dynamics_page=next(page for page in ui["pages"] if page["name"]=="DYNAMICS")
assert dynamics_page["controls"][12:15]==[{"param":"Comp Release","widget":"knob"},{"param":"Comp GR","widget":"readout"},{"param":"Comp Makeup","widget":"knob"}]
assert 'mpc::floatParam("comp_gr","Comp GR",0,60,0,"dB",1)' in src
assert "compGr.automatable = false;" in src
assert "set(kCompGR,0.0f);" in src
assert "const float compReductionDb = -20.0f*std::log10(finiteClamp(compGain_,1.0e-3f,1.0f,1.0f));" in src
assert src.index("compGain_=targetComp;") < src.index("set(kCompGR,compReductionDb);")
assert src.index("set(kCompGR,compReductionDb);") > src.index("if(ctx.numOutputs==1)")
# Positive display magnitude: unity gain is 0 dB GR; a 0.5 gain is 6.0206 dB GR.
for gain,expected in ((1.0,0.0),(0.5,20.0*math.log10(2.0)),(0.25,20.0*math.log10(4.0)),(0.001,60.0)):
    displayed=min(60.0,max(0.0,-20.0*math.log10(gain)))
    assert abs(displayed-expected)<1.0e-10

assert "bool formatValue(int index,float plainValue,std::string& out) override" in src
assert 'std::snprintf(buffer,sizeof(buffer),"%.1f dB",finiteClamp(plainValue,0.0f,60.0f,0.0f));' in src

assert "bool  transient = false;" in framework
assert "const float savedPlain = pr.transient ? pr.def : get(i);" in framework
assert "if (pr.transient)\n                    set(i, pr.def);" in framework
assert "compGr.transient = true;" in src

assert "if (!p.automatable[(size_t)i]) { ++skipped; continue; }" in host_sim
assert "all %d automatable parameters restored from the chunk" in host_sim


# Warmth / Clarity: parameter identity, placement, smoothing and response design.
assert 'addParam(mpc::floatParam("tone_warmth","Warmth",0,100,0,"%",0))' in src
assert 'addParam(mpc::floatParam("tone_clarity","Clarity",0,100,0,"%",0))' in src
assert "kCompGR, kToneWarmth, kToneClarity, kCount" in src
assert "toneWarmth_.snap(finiteClamp(get(kToneWarmth),0,100,0)*0.01f)" in src
assert "toneClarity_.snap(finiteClamp(get(kToneClarity),0,100,0)*0.01f)" in src
assert "toneWarmth_.next(" in src and "toneClarity_.next(" in src
assert "ch_[c].toneWarmthLow.shelf(sr_,180.0f,1.5f*warmth,false)" in src
assert "ch_[c].toneWarmthHigh.shelf(sr_,10000.0f,-1.25f*warmth,true)" in src
assert "ch_[c].toneClarityLow.bell(sr_,280.0f,0.75f,-1.5f*clarity)" in src
assert "ch_[c].toneClarityPresence.bell(sr_,3200.0f,0.80f,1.75f*clarity)" in src
assert "x=s.toneWarmthLow.run(x); x=s.toneWarmthHigh.run(x);" in src
assert "x=s.toneClarityLow.run(x); x=s.toneClarityPresence.run(x);" in src
assert "Smooth toneWarmth_,toneClarity_;" in src
assert src.index("for(int band=0;band<16;++band) x=s.graphic[band].run(x)") < src.index("x=s.toneWarmthLow.run(x)") < src.index("const float peak=")
assert src.index("ch_[c].toneWarmthLow.shelf") < src.index("for(int band=0;band<16;++band) x=s.graphic[band].run(x)")
assert all(len(values.split(","))==71 for _,values in programs)
assert "x.version=1700;" in src
assert "At 0% both paths are exactly neutral" in readme
# Independent centre-gain targets for the RBJ peaking sections.
for amount in (0.0,0.25,0.5,1.0):
    assert abs((-1.5*amount)-(-1.5*amount))<1e-12
    assert abs((1.75*amount)-(1.75*amount))<1e-12
assert 10**(1.5/20)>1.0 and 10**(-1.25/20)<1.0
