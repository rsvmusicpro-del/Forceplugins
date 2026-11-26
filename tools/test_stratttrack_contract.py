#!/usr/bin/env python3
"""StrattTrack 1.4.0 fixed graphic-EQ contract and independent DSP checks."""
import json,pathlib,re,math
root=pathlib.Path(__file__).resolve().parents[1]/"plugins"/"StrattTrack"
m=json.loads((root/"plugin.json").read_text()); u=json.loads((root/"ui.json").read_text())
s=(root/"src"/"StrattTrack.cpp").read_text(); n=(root/"DSP-NOTES.md").read_text()
r=(root/"README.md").read_text(); research=(root/"GRAPHIC-EQ-RESEARCH.md").read_text()
freq=[20.,31.5,50.,80.,125.,200.,315.,500.,800.,1250.,2000.,3150.,5000.,8000.,12500.,20000.]
names=["20 Hz","31.5 Hz","50 Hz","80 Hz","125 Hz","200 Hz","315 Hz","500 Hz","800 Hz","1.25 kHz","2 kHz","3.15 kHz","5 kHz","8 kHz","12.5 kHz","20 kHz"]
assert (m["name"],m["manufacturer"],m["version"],m["uid"])==("StrattTrack","Stratt","1.4.0","S4X3")
assert m["numInputs"]==m["numOutputs"]==2 and len(m["knobs"])==len(m["shiftKnobs"])==3
assert all(x["parameter"]<68 for x in m["knobs"]+m["shiftKnobs"])
assert "x.version=1400;" in s and len(re.findall(r"addParam\(",s))==68
assert "static const float kGraphicEqFrequencies[16]" in s and "kGraphicEqQ=2.146f" in s
assert "Biquad graphic[16]" in s and "Smooth graphicEqGain_[16]" in s
assert "ch_[c].graphic[band].bell(sr_,kGraphicEqFrequencies[band],kGraphicEqQ,graphicEqGains[band]);" in s
assert "for(int band=0;band<16;++band) x=s.graphic[band].run(x);" in s
assert "graphicEqGain_[band].next(" in s and "filterSmooth" in s and "for(int blockStart=0;blockStart<n;blockStart+=64)" in s
assert "for(int i=0;i<16;++i) graphic[i].reset();" in s
assert not any(x in s for x in ("s.lf.run","s.lm.run","s.hm.run","s.hf.run","ch_[c].lf.bell","ch_[c].lm.bell","ch_[c].hm.bell","ch_[c].hf.bell","lfFreq_.snap","lmFreq_.snap","hmFreq_.snap","hfFreq_.snap"))
assert "sixteen stereo-linked" in r and "sixteen stereo-linked RBJ peaking" in n
assert all(x in research for x in ("Pioneer DEH-P8850MP","Rane GQ 15","Voxengo Marvel GEQ","Voxengo Overtone GEQ","CamillaDSP","GPL-3.0"))
assert u["kit"]=="mpc-red" and u["qlinkGroups"]=="rows"
assert [p["name"] for p in u["pages"]]==["INPUT","FILTERS","PULTEC","EQ","DYNAMICS","OUTPUT"]
eq=next(p for p in u["pages"] if p["name"]=="EQ")
assert len(eq["controls"])==16 and [x["param"] for x in eq["controls"]]==names
assert all(x["widget"]=="fader" and x["polarity"]=="bipolar" for x in eq["controls"])
ids=[]
for line in s.splitlines():
 if "addParam(legacyInput)" in line: ids.append("input")
 else:
  z=re.search(r'addParam\(mpc::(?:float|log|bool|choice)Param\("([^"]+)"',line)
  if z: ids.append(z.group(1))
assert len(ids)==68
assert ids[:52]==["output","input","preamp","phase_l","phase_r","pan_l","pan_r","hp_freq","lp_freq","lf_freq","lf_gain","lm_freq","lm_gain","lm_q","hm_freq","hm_gain","hm_q","hf_freq","hf_gain","gate_thresh","gate_range","gate_attack","gate_release","comp_thresh","comp_ratio","comp_attack","comp_release","comp_makeup","drive","circuit_mix","out_pan","pt_lf_freq","pt_lf_boost","pt_lf_atten","pt_hf_freq","pt_hf_boost","pt_bandwidth","pt_hf_atten_freq","pt_hf_atten","console_fader","invert_lr","width","correlation","bass_mono","cs_hp_on","cs_hp_freq","cs_hp_order","cs_lp_on","cs_lp_freq","cs_lp_order","mono_compatibility","mono_threshold"]
assert ids[52:]==["eq_20","eq_31_5","eq_50","eq_80","eq_125","eq_200","eq_315","eq_500","eq_800","eq_1250","eq_2000","eq_3150","eq_5000","eq_8000","eq_12500","eq_20000"]
p=re.findall(r'addProgram\("([^"]+)",\s*\{([^}]*)\}\)',s)
assert len(p)>=2 and all(len(v.split(","))==68 for _,v in p)
assert [(x["parameter"],x["label"]) for x in m["knobs"]]==[(0,"Output"),(7,"HP Freq"),(8,"LP Freq")]
assert [(x["parameter"],x["label"]) for x in m["shiftKnobs"]]==[(2,"Preamp"),(28,"Drive"),(29,"Circuit")]
assert "for(int blockStart=0;blockStart<n;blockStart+=64)" in s and "x=s.hp.process(x); x=s.lp.process(x);" in s
assert "float csHpWet=s.csHp.process(x);" in s and "monoSafety_.next(monoSafetyTarget,sampleSmooth)" in s
def response(b,a,w):
 z=complex(math.cos(-w),math.sin(-w)); z2=z*z
 return abs((b[0]+b[1]*z+b[2]*z2)/(1+a[0]*z+a[1]*z2))
def peak(sr,fc,q,g):
 w=2*math.pi*fc/sr; c=math.cos(w); al=math.sin(w)/(2*q); A=10**(g/40); a0=1+al/A
 return ((1+al*A)/a0,-2*c/a0,(1-al*A)/a0),(-2*c/a0,(1-al/A)/a0),w
for sr in (44100.,48000.,96000.):
 for fc in freq:
  fc=min(fc,sr*.45)
  for gain in (-12.,0.,12.):
   b,a,w=peak(sr,fc,2.146,gain)
   assert abs(response(b,a,w)-10**(gain/20))<1e-8
   if gain==0:
    assert abs(response(b,a,0)-1)<1e-8 and abs(response(b,a,math.pi)-1)<1e-8
ratios=[freq[i+1]/freq[i] for i in range(15)]
assert max(ratios)-min(ratios)<.012
q=math.sqrt(2**(2/3))/(2**(2/3)-1); assert abs(q-2.146)<.01
for sr in (44100.,48000.,96000.):
 for fc in freq:
  b,a,w=peak(sr,min(fc,sr*.45),2.146,12); a1,a2=a; d=complex(a1*a1-4*a2)**.5
  assert max(abs((-a1+d)/2),abs((-a1-d)/2))<1
print("StrattTrack 16-band graphic EQ contract and independent DSP checks OK")
