// StrattTrack — transparent SSL-style stereo channel strip for MPC / Force.
// SPDX-License-Identifier: MIT
// Signal flow: input trim -> preamp -> L/R polarity -> dual input pan ->
// HP/LP -> four-band EQ -> gate -> compressor -> circuit colour (drive + fixed
// valve-inspired stage, parallel blend) -> output trim -> output balance -> fader.
//
// All clean stages use linear gain and stable cookbook biquads. Only the
// explicitly blended circuit stage is nonlinear. The fixed valve-inspired
// transfer is part of that same colour path and cannot be adjusted separately.
// No allocation, locks, I/O or per-sample transcendental functions occur in process().

#include "mpc_plugin.h"
#include <cmath>

namespace {
enum P {
    kOutput=0, kInput, kPreamp, kPhaseL, kPhaseR, kPanL, kPanR,
    kHpFreq, kLpFreq,
    kLfFreq, kLfGain, kLmFreq, kLmGain, kLmQ,
    kHmFreq, kHmGain, kHmQ, kHfFreq, kHfGain,
    kGateThresh, kGateRange, kGateAttack, kGateRelease,
    kCompThresh, kCompRatio, kCompAttack, kCompRelease, kCompMakeup,
    kDrive, kCircuitMix, kOutPan, kFader, kCount
};
constexpr float kPi=3.14159265358979323846f;
constexpr float kHalfPi=1.57079632679489661923f;
inline float db(float x) { return std::pow(10.0f, x*0.05f); }
inline float finiteClamp(float x,float lo,float hi,float fallback) {
    return std::isfinite(x) ? mpc::clamp(x,lo,hi) : fallback;
}
struct Smooth {
    float v=0.0f;
    float next(float target,float c=0.002f) { v += c*(target-v); return v; }
    void snap(float x) { v=x; }
};
struct Biquad {
    float b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;
    void set(float nb0,float nb1,float nb2,float na0,float na1,float na2) {
        if(!std::isfinite(nb0)||!std::isfinite(nb1)||!std::isfinite(nb2)||
           !std::isfinite(na0)||!std::isfinite(na1)||!std::isfinite(na2)||std::fabs(na0)<1.0e-12f) return;
        const float inv=1.0f/na0;
        b0=nb0*inv; b1=nb1*inv; b2=nb2*inv;
        a1=na1*inv; a2=na2*inv;
    }
    void bell(float sr,float f,float q,float gainDb) {
        f=finiteClamp(f,10.0f,sr*0.45f,1000.0f);
        q=finiteClamp(q,0.25f,8.0f,0.7071f);
        const float A=std::pow(10.0f,gainDb/40.0f);
        const float w=2.0f*kPi*f/sr, c=std::cos(w), s=std::sin(w);
        const float alpha=s/(2.0f*q);
        set(1+alpha*A,-2*c,1-alpha*A,1+alpha/A,-2*c,1-alpha/A);
    }
    void shelf(float sr,float f,float gainDb,bool high) {
        f=finiteClamp(f,10.0f,sr*0.45f,1000.0f);
        const float A=std::pow(10.0f,gainDb/40.0f);
        const float w=2.0f*kPi*f/sr, c=std::cos(w), s=std::sin(w);
        const float alpha=s*0.5f*1.41421356237f, beta=2.0f*std::sqrt(A)*alpha;
        if (!high)
            set(A*((A+1)-(A-1)*c+beta),2*A*((A-1)-(A+1)*c),
                A*((A+1)-(A-1)*c-beta),(A+1)+(A-1)*c+beta,
                -2*((A-1)+(A+1)*c),(A+1)+(A-1)*c-beta);
        else
            set(A*((A+1)+(A-1)*c+beta),-2*A*((A-1)+(A+1)*c),
                A*((A+1)+(A-1)*c-beta),(A+1)-(A-1)*c+beta,
                2*((A-1)-(A+1)*c),(A+1)-(A-1)*c-beta);
    }
    void pass(float sr,float f,bool high) {
        f=finiteClamp(f,10.0f,sr*0.45f,1000.0f);
        const float w=2.0f*kPi*f/sr,c=std::cos(w),s=std::sin(w);
        const float alpha=s/(2.0f*0.70710678f);
        if(high) set((1+c)*0.5f,-(1+c),(1+c)*0.5f,1+alpha,-2*c,1-alpha);
        else set((1-c)*0.5f,1-c,(1-c)*0.5f,1+alpha,-2*c,1-alpha);
    }
    float run(float x) {
        const float y=b0*x+z1;
        z1=b1*x-a1*y+z2; z2=b2*x-a2*y;
        if(!std::isfinite(y)||!std::isfinite(z1)||!std::isfinite(z2)){z1=z2=0;return 0;}
        return y;
    }
    void reset(){z1=z2=0;}
};
struct Channel {
    Biquad hp,lp,lf,lm,hm,hf;
    float valveDc=0.0f, valvePrev=0.0f;
    void reset(){hp.reset();lp.reset();lf.reset();lm.reset();hm.reset();hf.reset();
        valveDc=valvePrev=0;}
};
inline float coeff(float ms,float sr) {
    ms=finiteClamp(ms,0.1f,2000.0f,10.0f);
    return std::exp(-1.0f/(0.001f*ms*sr));
}
inline float panL(float p) { return std::cos((p+1.0f)*kHalfPi*0.5f); }
inline float panR(float p) { return std::sin((p+1.0f)*kHalfPi*0.5f); }
// Bounded rational soft saturation; no per-sample tanh/exp. The fixed,
// asymmetric valve term is intentionally inside this one colour block.
inline float circuit(float x,float drive) {
    const float d=1.0f+drive*5.0f;
    const float u=x*d;
    const float shaped=u/(1.0f+0.22f*drive*std::fabs(u));
    return shaped/d; // unity small-signal gain while Drive changes only the nonlinear knee.
}
}

class StrattTrack : public mpc::Plugin {
public:
    StrattTrack():mpc::Plugin(makeInfo()) {
        addParam(mpc::floatParam("output","Output",-24,24,0,"dB",1));
        addParam(mpc::floatParam("input","Input",-24,24,0,"dB",1));
        addParam(mpc::floatParam("preamp","Preamp",-18,18,0,"dB",1));
        addParam(mpc::boolParam("phase_l","Phase L",false));
        addParam(mpc::boolParam("phase_r","Phase R",false));
        addParam(mpc::floatParam("pan_l","Pan L",-100,100,-100,"%",0));
        addParam(mpc::floatParam("pan_r","Pan R",-100,100,100,"%",0));
        addParam(mpc::logParam("hp_freq","HP Freq",20,1000,20,"Hz",0));
        addParam(mpc::logParam("lp_freq","LP Freq",1000,22000,22000,"Hz",0));
        addParam(mpc::logParam("lf_freq","LF Freq",30,450,100,"Hz",0));
        addParam(mpc::floatParam("lf_gain","LF Gain",-15,15,0,"dB",1));
        addParam(mpc::logParam("lm_freq","LM Freq",100,2000,400,"Hz",0));
        addParam(mpc::floatParam("lm_gain","LM Gain",-15,15,0,"dB",1));
        addParam(mpc::floatParam("lm_q","LM Q",0.4f,4.0f,1.0f,"Q",2));
        addParam(mpc::logParam("hm_freq","HM Freq",500,8000,2500,"Hz",0));
        addParam(mpc::floatParam("hm_gain","HM Gain",-15,15,0,"dB",1));
        addParam(mpc::floatParam("hm_q","HM Q",0.4f,4.0f,1.0f,"Q",2));
        addParam(mpc::logParam("hf_freq","HF Freq",2000,20000,12000,"Hz",0));
        addParam(mpc::floatParam("hf_gain","HF Gain",-15,15,0,"dB",1));
        addParam(mpc::floatParam("gate_thresh","Gate Thresh",-80,0,-80,"dB",1));
        addParam(mpc::floatParam("gate_range","Gate Range",0,80,60,"dB",1));
        addParam(mpc::floatParam("gate_attack","Gate Attack",0.1f,100,1,"ms",1));
        addParam(mpc::floatParam("gate_release","Gate Release",5,1000,120,"ms",1));
        addParam(mpc::floatParam("comp_thresh","Comp Thresh",-40,0,0,"dB",1));
        addParam(mpc::floatParam("comp_ratio","Comp Ratio",1,20,2," :1",1));
        addParam(mpc::floatParam("comp_attack","Comp Attack",0.1f,100,10,"ms",1));
        addParam(mpc::floatParam("comp_release","Comp Release",5,1000,100,"ms",1));
        addParam(mpc::floatParam("comp_makeup","Comp Makeup",0,24,0,"dB",1));
        addParam(mpc::floatParam("drive","Drive",0,100,0,"%",0));
        addParam(mpc::floatParam("circuit_mix","Circuit Mix",0,100,0,"%",0));
        addParam(mpc::floatParam("out_pan","Out Pan",-100,100,0,"%",0));
        addParam(mpc::floatParam("fader","Fader",-60,12,0,"dB",1));
        addProgram("Clean", {0,0,0,0,0,-100,100,20,22000,100,0,400,0,1,2500,0,1,12000,0,-80,60,1,120,0,2,10,100,0,0,0,0,0});
        addProgram("Vocal", {0,0,0,0,0,-100,100,80,18000,120,1,350,-2,1.2f,3200,2,1.1f,12000,1,-45,35,1,100,-18,3,8,90,2,12,25,0,0});
        // Precompute 1:1..20:1 compressor curves before audio starts. Runtime
        // ratio automation only interpolates adjacent curves; it never calls pow().
        for(int r=0;r<20;++r) {
            const float exponent=-(1.0f-1.0f/static_cast<float>(r+1));
            for(int i=0;i<=256;++i) {
                const float over=1.0f+(2047.0f/256.0f)*static_cast<float>(i);
                compCurve_[r][i]=std::pow(over,exponent);
            }
        }
    }
    void prepare(double sr,int) override { sr_=(float)sr; valveDcCoef_=std::exp(-2.0f*kPi*3.0f/sr_); reset(); }
    void reset() override {
        for(auto &c:ch_) c.reset();
        detector_=0.0f; gateGain_=compGain_=1.0f; gateOpen_=true;
        output_.snap(db(get(kOutput))); input_.snap(db(get(kInput)));
        preamp_.snap(db(get(kPreamp))); fader_.snap(db(get(kFader)));
        circuitMix_.snap(get(kCircuitMix)*0.01f);
        hpFreq_.snap(get(kHpFreq)); lpFreq_.snap(get(kLpFreq));
        lfFreq_.snap(get(kLfFreq)); lfGain_.snap(get(kLfGain));
        lmFreq_.snap(get(kLmFreq)); lmGain_.snap(get(kLmGain)); lmQ_.snap(get(kLmQ));
        hmFreq_.snap(get(kHmFreq)); hmGain_.snap(get(kHmGain)); hmQ_.snap(get(kHmQ));
        hfFreq_.snap(get(kHfFreq)); hfGain_.snap(get(kHfGain));
    }
    void process(mpc::ProcessContext& ctx) override {
        const int n=mpc::clamp(ctx.numFrames,0,2048);
        if(ctx.outputs==nullptr||ctx.numOutputs<1||ctx.outputs[0]==nullptr) return;
        const float inGain=db(finiteClamp(get(kInput),-24,24,0));
        const float preampGain=db(finiteClamp(get(kPreamp),-18,18,0));
        const float outGain=db(finiteClamp(get(kOutput),-24,24,0));
        const float faderGain=db(finiteClamp(get(kFader),-60,12,0));
        const float gateThreshold=db(finiteClamp(get(kGateThresh),-80,0,-80));
        const float gateFloor=db(-finiteClamp(get(kGateRange),0,80,60));
        const float compThreshold=db(finiteClamp(get(kCompThresh),-40,0,0));
        const float ratio=finiteClamp(get(kCompRatio),1,20,1);
        const float gateAtk=coeff(get(kGateAttack),sr_), gateRel=coeff(get(kGateRelease),sr_);
        const float compAtk=coeff(get(kCompAttack),sr_), compRel=coeff(get(kCompRelease),sr_);
        const float makeup=db(finiteClamp(get(kCompMakeup),0,24,0));
remove runtime LUT rebuild        const float drive=finiteClamp(get(kDrive)*0.01f,0,1,0);
        const float cmix=finiteClamp(get(kCircuitMix)*0.01f,0,1,0);
        const float inL=finiteClamp(get(kPanL)*0.01f,-1,1,-1), inR=finiteClamp(get(kPanR)*0.01f,-1,1,1);
        const float ilL=panL(inL), ilR=panR(inL), irL=panL(inR), irR=panR(inR);
        const float op=finiteClamp(get(kOutPan)*0.01f,-1,1,0);
        const float oL=op>0?std::cos(op*kHalfPi*0.5f):1.0f;
        const float oR=op<0?std::cos(-op*kHalfPi*0.5f):1.0f;
        const float controlSmooth=1.0f-std::exp(-static_cast<float>(n)/(0.010f*std::fmax(sr_,1.0f)));
        const float hp=hpFreq_.next(finiteClamp(get(kHpFreq),20,0.45f*sr_,20),controlSmooth);
        const float lp=lpFreq_.next(finiteClamp(get(kLpFreq),1000,0.49f*sr_,0.49f*sr_),controlSmooth);
        const float lfF=lfFreq_.next(finiteClamp(get(kLfFreq),30,450,100),controlSmooth), lfG=lfGain_.next(finiteClamp(get(kLfGain),-15,15,0),controlSmooth);
        const float lmF=lmFreq_.next(finiteClamp(get(kLmFreq),100,2000,400),controlSmooth), lmG=lmGain_.next(finiteClamp(get(kLmGain),-15,15,0),controlSmooth), lmQv=lmQ_.next(finiteClamp(get(kLmQ),0.4f,4,1),controlSmooth);
        const float hmF=hmFreq_.next(finiteClamp(get(kHmFreq),500,8000,2500),controlSmooth), hmG=hmGain_.next(finiteClamp(get(kHmGain),-15,15,0),controlSmooth), hmQv=hmQ_.next(finiteClamp(get(kHmQ),0.4f,4,1),controlSmooth);
        const float hfF=hfFreq_.next(finiteClamp(get(kHfFreq),2000,20000,12000),controlSmooth), hfG=hfGain_.next(finiteClamp(get(kHfGain),-15,15,0),controlSmooth);
        for(int c=0;c<2;++c) {
            ch_[c].hp.pass(sr_,hp,true); ch_[c].lp.pass(sr_,lp,false);
            ch_[c].lf.shelf(sr_,lfF,lfG,false);
            ch_[c].lm.bell(sr_,lmF,lmQv,lmG);
            ch_[c].hm.bell(sr_,hmF,hmQv,hmG);
            ch_[c].hf.shelf(sr_,hfF,hfG,true);
        }
        const float phaseL=getBool(kPhaseL)?-1.0f:1.0f, phaseR=getBool(kPhaseR)?-1.0f:1.0f;
        for(int i=0;i<n;++i) {
            const float ig=input_.next(inGain), og=output_.next(outGain);
            const float pg=preamp_.next(preampGain), fg=fader_.next(faderGain);
            const float mix=circuitMix_.next(cmix);
            const float l=(ctx.inputs&&ctx.numInputs>0&&ctx.inputs[0])?ctx.inputs[0][i]:0.0f;
            const float r=(ctx.inputs&&ctx.numInputs>1&&ctx.inputs[1])?ctx.inputs[1][i]:0.0f;
            const float a=finiteClamp(l,-16,16,0)*ig*pg*phaseL;
            const float b=finiteClamp(r,-16,16,0)*ig*pg*phaseR;
            float xL=(a*ilL+b*irL), xR=(a*ilR+b*irR);
            float *samples[2]={&xL,&xR};
            // Clean filtering and EQ precede the linked dynamics detector.
            for(int ch=0;ch<2;++ch) {
                Channel &s=ch_[ch]; float x=*samples[ch];
                x=s.hp.run(x); x=s.lp.run(x);
                x=s.lf.run(x); x=s.lm.run(x); x=s.hm.run(x); x=s.hf.run(x);
                *samples[ch]=x;
            }
            // One linked-stereo dynamics detector/gain computer: both channels
            // receive exactly the same gain reduction and gate envelope.
            const float peak=std::fmax(std::fabs(xL),std::fabs(xR));
            const float envCoef=peak>detector_?compAtk:compRel;
            detector_=envCoef*detector_+(1.0f-envCoef)*peak;
            const float over=detector_/std::fmax(compThreshold,1.0e-9f);
            // Conventional hard-knee compressor law in the amplitude domain.
            const float curvePos=finiteClamp((over-1.0f)*(256.0f/2047.0f),0.0f,256.0f,0.0f);
            const int curveIndex=static_cast<int>(curvePos);
            const float curveFrac=curvePos-static_cast<float>(curveIndex);
            const float ratioPos=ratio-1.0f;
            const int ratioIndex=mpc::clamp(static_cast<int>(ratioPos),0,19);
            const float ratioFrac=ratioPos-static_cast<float>(ratioIndex);
            const int nextRatio=ratioIndex<19?ratioIndex+1:19;
            const float curveLo=compCurve_[ratioIndex][curveIndex]+
                (compCurve_[ratioIndex][curveIndex<256?curveIndex+1:256]-compCurve_[ratioIndex][curveIndex])*curveFrac;
            const float curveHi=compCurve_[nextRatio][curveIndex]+
                (compCurve_[nextRatio][curveIndex<256?curveIndex+1:256]-compCurve_[nextRatio][curveIndex])*curveFrac;
            const float targetComp=curveLo+(curveHi-curveLo)*ratioFrac;
            const float cc=targetComp<compGain_?compAtk:compRel;
            compGain_=cc*compGain_+(1.0f-cc)*targetComp;
            // 3 dB hysteresis prevents gate chatter around its opening threshold.
            const float gateClose=gateThreshold*db(-3.0f);
            if(gateOpen_ ? detector_<gateClose : detector_>gateThreshold) gateOpen_=!gateOpen_;
            const float gateTarget=gateOpen_?1.0f:gateFloor;
            const float gc=gateTarget<gateGain_?gateAtk:gateRel;
            gateGain_=gc*gateGain_+(1.0f-gc)*gateTarget;
            for(int ch=0;ch<2;++ch) {
                Channel &s=ch_[ch];
                *samples[ch]=*samples[ch]*gateGain_*compGain_*makeup;
                const float clean=*samples[ch];
                // Circuit and valve transfer form one independently blended colour engine.
                // At Mix=0 the output is mathematically the clean dynamics signal.
                const float driveWet=circuit(clean,drive);
                const float valveIn=driveWet*1.04f;
                const float valveStatic=(valveIn/(1.0f+0.035f*std::fabs(valveIn))
                                         +0.003f*valveIn*valveIn)/1.04f;
                s.valveDc=valveDcCoef_*(s.valveDc+valveStatic-s.valvePrev);
                s.valvePrev=valveStatic;
                const float colour=s.valveDc;
                *samples[ch]=clean+(colour-clean)*mix;
            }
            xL*=og*fg; xR*=og*fg;
            if(ctx.numOutputs==1) ctx.outputs[0][i]=0.5f*(xL+xR);
            else {
                if(ctx.outputs[0]) ctx.outputs[0][i]=xL*oL;
                if(ctx.outputs[1]) ctx.outputs[1][i]=xR*oR;
                for(int c=2;c<ctx.numOutputs;++c) if(ctx.outputs[c]) ctx.outputs[c][i]=0.0f;
            }
        }
    }
private:
    static mpc::PluginInfo makeInfo() {
        mpc::PluginInfo x; x.name="StrattTrack"; x.vendor="Stratt";
        x.uniqueId=MPC_FOURCC('S','4','X','3'); x.version=1000;
        x.isInstrument=false; x.numInputs=2; x.numOutputs=2; x.tailSamples=1; return x;
    }
    Channel ch_[2]; float sr_=44100;
    Smooth output_,input_,preamp_,fader_,circuitMix_;
    Smooth hpFreq_,lpFreq_,lfFreq_,lfGain_,lmFreq_,lmGain_,lmQ_,hmFreq_,hmGain_,hmQ_,hfFreq_,hfGain_;
    // Linked dynamics state is separate from per-channel filter/colour state.
    float detector_=0.0f, gateGain_=1.0f, compGain_=1.0f;
    bool gateOpen_=true;
    float compCurve_[20][257]{};
    float valveDcCoef_=0.999572f;
};
MPC_PLUGIN_ENTRY(StrattTrack)
