// StrattTrack — transparent SSL-style stereo channel strip for MPC / Force.
// SPDX-License-Identifier: MIT
// Signal flow: Airwindows Console7 input stage -> preamp -> L/R polarity -> dual input pan ->
// HP/LP -> serial Cleansweep HP/LP -> Pulse Techniques EQP-1A curve model -> four-band EQ -> gate ->
// compressor -> user-controlled drive colour -> Mono Compatibility -> permanent
// invisible ADAA valve stage -> output balance -> tone-neutral master Output gain.
//
// All clean stages use linear gain and stable cookbook biquads. The Drive/Circuit
// control affects only its own parallel colour path; the fixed valve stage is
// separate, permanently active, and has no parameter or UI mapping.
// No allocation, locks, I/O or per-sample transcendental functions occur in process().

#include "mpc_plugin.h"
#include "dsp/two_pole_filter.h"
#include "cleansweep_cascade.h"
#include "dsp/airwindows_console7.h"
#include <cmath>

namespace {
enum P {
    kOutput=0, kLegacyInputSlot, kPreamp, kPhaseL, kPhaseR, kPanL, kPanR,
    kHpFreq, kLpFreq,
    kLfFreq, kLfGain, kLmFreq, kLmGain, kLmQ,
    kHmFreq, kHmGain, kHmQ, kHfFreq, kHfGain,
    kGateThresh, kGateRange, kGateAttack, kGateRelease,
    kCompThresh, kCompRatio, kCompAttack, kCompRelease, kCompMakeup,
    kDrive, kCircuitMix, kOutPan,
    kPtLowFreq, kPtLowBoost, kPtLowAtten, kPtHighFreq,
    kPtHighBoost, kPtBandwidth, kPtHighAttenFreq, kPtHighAtten, kConsoleFader, kInvertLR, kWidth, kCorrelation, kBassMono,
    kCsHpEnable, kCsHpFreq, kCsHpOrder, kCsLpEnable, kCsLpFreq, kCsLpOrder,
    kMonoCompatibility, kMonoThreshold,
    kGraphicEq01, kGraphicEq02, kGraphicEq03, kGraphicEq04, kGraphicEq05, kGraphicEq06, kGraphicEq07, kGraphicEq08,
    kGraphicEq09, kGraphicEq10, kGraphicEq11, kGraphicEq12, kGraphicEq13, kGraphicEq14, kGraphicEq15, kGraphicEq16, kCount
};
static const float kGraphicEqFrequencies[16] = {20.0f,31.7f,50.2f,79.6f,126.0f,200.0f,317.0f,502.0f,796.0f,1260.0f,2000.0f,3170.0f,5020.0f,7960.0f,12600.0f,20000.0f};
constexpr float kGraphicEqQ=2.146f;
constexpr float kPi=3.14159265358979323846f;
constexpr float kHalfPi=1.57079632679489661923f;
inline float db(float x) { return std::pow(10.0f, x*0.05f); }
inline float finiteClamp(float x,float lo,float hi,float fallback) {
    return std::isfinite(x) ? mpc::clamp(x,lo,hi) : fallback;
}
struct Smooth {
    float v=0.0f;
    float next(float target,float c) {
        if(!std::isfinite(v)) v=std::isfinite(target)?target:0.0f;
        if(!std::isfinite(target)) target=v;
        c=finiteClamp(c,0.0f,1.0f,0.0f);
        v += c*(target-v);
        if(!std::isfinite(v)) v=target;
        return v;
    }
    void snap(float x) { v=std::isfinite(x)?x:0.0f; }
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
    // Parallel-RLC resonant boost: direct path plus a unity-peak band-pass.
    void resonantBoost(float sr,float f,float q,float gainDb) {
        f=finiteClamp(f,10.0f,sr*0.45f,1000.0f);
        q=finiteClamp(q,0.25f,8.0f,0.7071f);
        const float w=2.0f*kPi*f/sr, c=std::cos(w), s=std::sin(w);
        const float alpha=s/(2.0f*q), a0=1.0f+alpha, a1=-2.0f*c, a2=1.0f-alpha;
        const float branch=std::pow(10.0f,gainDb/20.0f)-1.0f;
        set(a0+branch*alpha,a1,a2-branch*alpha,a0,a1,a2);
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
// Even antiderivative of softClip(). The polynomial is integrated exactly,
// so the ADAA divided difference remains the analytic interval average.
static inline double softClipIntegral(double x)
{
    const double a = std::fabs(x);
    if (a <= 1.0) return 0.5 * a * a;
    if (a >= 2.0) return 1.85 + 1.5 * (a - 2.0);
    const double t = a - 1.0;
    const double t2 = t * t;
    return 0.5 + t + 0.5 * t2 - 0.25 * t2 * t2 + 0.1 * t2 * t2 * t;
}

// Integral of softClip(x)^2, required for the asymmetric even-harmonic term.
// The transition polynomial is integrated analytically (degree 9).
static inline double softClipSquareIntegral(double x)
{
    const double a = std::fabs(x);
    if (a <= 1.0)
        return std::copysign(a * a * a / 3.0, x);
    if (a >= 2.0)
        return std::copysign((5491.0 / 2520.0) + 2.25 * (a - 2.0), x);

    const double t = a - 1.0;
    const double t2 = t * t;
    const double t3 = t2 * t;
    const double t4 = t2 * t2;
    const double t5 = t4 * t;
    const double t6 = t3 * t3;
    const double t7 = t6 * t;
    const double t8 = t4 * t4;
    const double t9 = t8 * t;
    const double integral = t + t2 + t3 / 3.0 - 0.5 * t4 - 0.2 * t5
                          + t6 / 6.0 + t7 / 7.0 - t8 / 8.0 + t9 / 36.0;
    return std::copysign(1.0 / 3.0 + integral, x);
}

// Point value of the asymmetric static transfer used by the ADAA integrator.
static inline double softClipTransfer(double x, double asymmetry)
{
    const double a = std::fabs(x);
    double shaped;
    if (a <= 1.0) shaped = x;
    else if (a >= 2.0) shaped = std::copysign(1.5, x);
    else
    {
        const double t = a - 1.0;
        shaped = std::copysign(1.0 + t - t * t * t + 0.5 * t * t * t * t, x);
    }
    return shaped + asymmetry * shaped * shaped;
}

constexpr double kTriodeBlend=0.28;
constexpr double kMediumMuScale=1.18;
constexpr double kHighMuAsymmetry=0.032;
constexpr double kMediumMuAsymmetry=0.060;

// Normalized ECC83-like high-mu + 6SN7-like medium-mu curve blend.
static inline double tubeTransfer(double x)
{
    return (1.0-kTriodeBlend)*softClipTransfer(x,kHighMuAsymmetry)
         +kTriodeBlend*softClipTransfer(x*kMediumMuScale,kMediumMuAsymmetry)/kMediumMuScale;
}
static inline double tubeIntegral(double x)
{
    const double k=kMediumMuScale;
    const double hi=softClipIntegral(x)+kHighMuAsymmetry*softClipSquareIntegral(x);
    const double med=softClipIntegral(k*x)/(k*k)
                    +kMediumMuAsymmetry*softClipSquareIntegral(k*x)/(k*k);
    return (1.0-kTriodeBlend)*hi+kTriodeBlend*med;
}

// Five-point Gauss-Legendre is exact for each degree-eight polynomial segment.
static inline double integrateShortTubeInterval(double lo,double hi)
{
    constexpr double nodes[2]={0.53846931010568309104,0.90617984593866399280};
    constexpr double weights[3]={0.56888888888888888889,0.47862867049936646804,0.23692688505618908751};
    constexpr double boundaries[9]={-2.0,-2.0/kMediumMuScale,-1.0,-1.0/kMediumMuScale,
                                     0.0,1.0/kMediumMuScale,1.0,2.0/kMediumMuScale,2.0};
    double integral=0.0, start=lo;
    for(double boundary:boundaries) {
        if(boundary<=start||boundary>=hi) continue;
        const double mid=0.5*(start+boundary), half=0.5*(boundary-start);
        const double p0=tubeTransfer(mid-nodes[0]*half)+tubeTransfer(mid+nodes[0]*half);
        const double p1=tubeTransfer(mid-nodes[1]*half)+tubeTransfer(mid+nodes[1]*half);
        integral+=half*(weights[0]*tubeTransfer(mid)+weights[1]*p0+weights[2]*p1);
        start=boundary;
    }
    const double mid=0.5*(start+hi), half=0.5*(hi-start);
    const double p0=tubeTransfer(mid-nodes[0]*half)+tubeTransfer(mid+nodes[0]*half);
    const double p1=tubeTransfer(mid-nodes[1]*half)+tubeTransfer(mid+nodes[1]*half);
    return integral+half*(weights[0]*tubeTransfer(mid)+weights[1]*p0+weights[2]*p1);
}

// First-order ADAA, with quadrature for very short intervals to avoid cancellation.
static inline float adaaTube(float x,float previousX)
{
    const double xd=x, pd=previousX, delta=xd-pd, ad=std::fabs(delta);
    if(ad==0.0) return static_cast<float>(tubeTransfer(xd));
    const double scale=std::fmax(1.0,std::fmax(std::fabs(xd),std::fabs(pd)));
    if(ad<=1.0e-4*scale) {
        const double ax=std::fabs(xd), ap=std::fabs(pd);
        const double midpoint=0.5*(xd+pd);
        const double d2=delta*delta;
        // Quiet programme samples remain in the two linear branches: exact
        // quadratic ADAA avoids the multi-node fallback on the common path.
        if(ax<=1.0/kMediumMuScale && ap<=1.0/kMediumMuScale) {
            const double eta=(1.0-kTriodeBlend)*kHighMuAsymmetry
                            +kTriodeBlend*kMediumMuAsymmetry*kMediumMuScale;
            return static_cast<float>(midpoint+eta*(midpoint*midpoint+d2/12.0));
        }
        // Both branches are constant beyond the outer knee on the same polarity.
        if(ax>=2.0 && ap>=2.0 && std::signbit(xd)==std::signbit(pd)) {
            const double sign=std::signbit(xd)?-1.0:1.0;
            const double plateau=(1.0-kTriodeBlend)*(sign*1.5+kHighMuAsymmetry*2.25)
                +kTriodeBlend*(sign*1.5+kMediumMuAsymmetry*2.25)/kMediumMuScale;
            return static_cast<float>(plateau);
        }
        const double lo=std::fmin(xd,pd), hi=std::fmax(xd,pd);
        return static_cast<float>(integrateShortTubeInterval(lo,hi)/(hi-lo));
    }
    return static_cast<float>((tubeIntegral(xd)-tubeIntegral(pd))/delta);
}


struct Channel {
    mpc::dsp::TwoPoleButterworth hp{mpc::dsp::TwoPoleButterworth::Mode::Highpass};
    mpc::dsp::TwoPoleButterworth lp{mpc::dsp::TwoPoleButterworth::Mode::Lowpass};
    Biquad graphic[16],ptHighBoost,ptLfInteraction;
    stratt::dsp::Console7Channel console;
    stratt::dsp::CleansweepButterworth csHp{stratt::dsp::CleansweepButterworth::Mode::Highpass};
    stratt::dsp::CleansweepButterworth csHpOld{stratt::dsp::CleansweepButterworth::Mode::Highpass};
    stratt::dsp::CleansweepButterworth csLp{stratt::dsp::CleansweepButterworth::Mode::Lowpass};
    stratt::dsp::CleansweepButterworth csLpOld{stratt::dsp::CleansweepButterworth::Mode::Lowpass};
    float ptLowBoostState=0,ptLowAttenState=0,ptHighAttenState=0;
    float previousTubeInput=0.0f;
    bool hasPreviousTubeInput=false;
    float previousTubeColor=0.0f;
    float previousTubeDcInput=0.0f, previousTubeDcOutput=0.0f;
    void reset(){console.reset();hp.reset();lp.reset();csHp.reset();csHpOld.reset();csLp.reset();csLpOld.reset();for(int i=0;i<16;++i) graphic[i].reset();
        ptHighBoost.reset();ptLfInteraction.reset();
        ptLowBoostState=ptLowAttenState=ptHighAttenState=0;
        previousTubeInput=previousTubeColor=previousTubeDcInput=previousTubeDcOutput=0.0f;
        hasPreviousTubeInput=false;}
    float tubeStage(float x,float pole,float hfAlpha) {
        // Fail closed on corrupted upstream state and bound the ADAA input so
        // one pathological sample cannot poison the antiderivative or DC memory.
        if(!std::isfinite(x)) {
            previousTubeInput=previousTubeColor=0.0f;
            previousTubeDcInput=previousTubeDcOutput=0.0f;
            hasPreviousTubeInput=false;
            return 0.0f;
        }
        x=mpc::clamp(x,-64.0f,64.0f);
        constexpr float tubeDrive=1.15f;
        const float driven=x*tubeDrive;
        const float previous=hasPreviousTubeInput?previousTubeInput:driven;
        const float shaped=adaaTube(driven,previous)/tubeDrive;
        previousTubeInput=driven;
        hasPreviousTubeInput=true;
        // Filter only the nonlinear residual; preserve the unity small-signal path.
        const float color=shaped-x;
        previousTubeColor+=hfAlpha*(color-previousTubeColor);
        const float voiced=x+0.72f*previousTubeColor+0.28f*color;
        const float y=voiced-previousTubeDcInput+pole*previousTubeDcOutput;
        previousTubeDcInput=voiced;
        previousTubeDcOutput=y;
        return y;
    }
};
inline float coeff(float ms,float sr) {
    ms=finiteClamp(ms,0.1f,2000.0f,10.0f);
    return std::exp(-1.0f/(0.001f*ms*sr));
}
inline float panL(float p) { return std::cos((p+1.0f)*kHalfPi*0.5f); }
inline float panR(float p) { return std::sin((p+1.0f)*kHalfPi*0.5f); }
inline float smoothLog(float x,float t,float a){x=finiteClamp(x,1,192000,1000);t=finiteClamp(t,1,192000,x);return std::exp(std::log(x)+a*(std::log(t)-std::log(x)));}
inline float ptLowFrequency(int i){static const float f[]={20,30,60,100};return f[mpc::clamp(i,0,3)];}
inline float ptHighFrequency(int i){static const float f[]={3000,4000,5000,8000,10000,12000,16000};return f[mpc::clamp(i,0,6)];}
inline float ptHighAttenFrequency(int i){static const float f[]={5000,10000,20000};return f[mpc::clamp(i,0,2)];}
// Bounded rational saturation for the user-controlled Drive/Circuit path.
// The permanent asymmetric ADAA valve stage is applied separately afterwards.
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
        // Retired Input slot: inert compatibility tombstone; preserve all later VST parameter indices and saved-state IDs.
        mpc::Param legacyInput=mpc::floatParam("input","Unused",0.0f,0.0f,0.0f,"",0);
        legacyInput.automatable=false;
        addParam(legacyInput);
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
        // Pulse Techniques EQP-1A controls appended after the original stable IDs.
        addParam(mpc::choiceParam("pt_lf_freq","PT LF Freq",{"20","30","60","100"},2));
        addParam(mpc::floatParam("pt_lf_boost","PT LF Boost",0,13.5f,0,"dB",1));
        addParam(mpc::floatParam("pt_lf_atten","PT LF Atten",0,17.5f,0,"dB",1));
        addParam(mpc::choiceParam("pt_hf_freq","PT HF Freq",{"3k","4k","5k","8k","10k","12k","16k"},2));
        addParam(mpc::floatParam("pt_hf_boost","PT HF Boost",0,18,0,"dB",1));
        addParam(mpc::floatParam("pt_bandwidth","PT Bandwidth",0,100,50,"%",0));
        addParam(mpc::choiceParam("pt_hf_atten_freq","PT HF Att",{"5k","10k","20k"},1));
        addParam(mpc::floatParam("pt_hf_atten","PT HF Atten",0,16,0,"dB",1));
        addParam(mpc::floatParam("console_fader","Console Fader",0,1,0.772f,"",3));
        // Appended controls preserve every existing parameter ID and index.
        addParam(mpc::boolParam("invert_lr","Invert L-R",false));
        addParam(mpc::floatParam("width","Width",0,200,100,"%",0));
        addParam(mpc::floatParam("correlation","Correlation",0,100,0,"%",0));
        addParam(mpc::logParam("bass_mono","Bass Mono",20,400,120,"Hz",0));
        // Independent second serial filter bank; disabled by default for transparent recall.
        addParam(mpc::boolParam("cs_hp_on","CS HP On",false));
        addParam(mpc::logParam("cs_hp_freq","CS HP Freq",20,11000,20,"Hz",0));
        addParam(mpc::choiceParam("cs_hp_order","CS HP Ord",{"1","2","3","4","5","6"},0));
        addParam(mpc::boolParam("cs_lp_on","CS LP On",false));
        addParam(mpc::logParam("cs_lp_freq","CS LP Freq",20,22000,22000,"Hz",0));
        addParam(mpc::choiceParam("cs_lp_order","CS LP Ord",{"1","2","3","4","5","6"},0));
        // Appended mono-protection controls preserve all existing parameter indices.
        addParam(mpc::choiceParam("mono_compatibility","Mono Compatibility",{"Off","Auto","Full"},0));
        addParam(mpc::floatParam("mono_threshold","Mono Threshold",0,100,50,"%",0));
        // New fixed-frequency graphic EQ controls are appended after the 52 legacy slots.
        addParam(mpc::floatParam("eq_20","20 Hz",-12,12,0,"dB",1));
        addParam(mpc::floatParam("eq_31_7","31.7 Hz",-12,12,0,"dB",1));
        addParam(mpc::floatParam("eq_50_2","50.2 Hz",-12,12,0,"dB",1));
        addParam(mpc::floatParam("eq_79_6","79.6 Hz",-12,12,0,"dB",1));
        addParam(mpc::floatParam("eq_126","126 Hz",-12,12,0,"dB",1));
        addParam(mpc::floatParam("eq_200","200 Hz",-12,12,0,"dB",1));
        addParam(mpc::floatParam("eq_317","317 Hz",-12,12,0,"dB",1));
        addParam(mpc::floatParam("eq_502","502 Hz",-12,12,0,"dB",1));
        addParam(mpc::floatParam("eq_796","796 Hz",-12,12,0,"dB",1));
        addParam(mpc::floatParam("eq_1260","1.26 kHz",-12,12,0,"dB",1));
        addParam(mpc::floatParam("eq_2000","2 kHz",-12,12,0,"dB",1));
        addParam(mpc::floatParam("eq_3170","3.17 kHz",-12,12,0,"dB",1));
        addParam(mpc::floatParam("eq_5020","5.02 kHz",-12,12,0,"dB",1));
        addParam(mpc::floatParam("eq_7960","7.96 kHz",-12,12,0,"dB",1));
        addParam(mpc::floatParam("eq_12600","12.6 kHz",-12,12,0,"dB",1));
        addParam(mpc::floatParam("eq_20000","20 kHz",-12,12,0,"dB",1));
        addProgram("Clean", {0, 0, 0, 0, 0, -100, 100, 20, 22000, 100, 0, 400, 0, 1, 2500, 0, 1, 12000, 0, -80, 60, 1, 120, 0, 2, 10, 100, 0, 0, 0, 0, 2, 0, 0, 2, 0, 50, 1, 0, 0.772f, 0, 100, 0, 120, 0, 20, 0, 0, 22000, 0, 0, 50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
        addProgram("Vocal", {0, 0, 0, 0, 0, -100, 100, 80, 18000, 120, 1, 350, -2, 1.2f, 3200, 2, 1.1f, 12000, 1, -45, 35, 1, 100, -18, 3, 8, 90, 2, 12, 25, 0, 2, 0, 0, 2, 0, 50, 1, 0, 0.772f, 0, 100, 0, 120, 0, 20, 0, 0, 22000, 0, 0, 50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    }
    void prepare(double sr,int) override {
        sr_=(std::isfinite(sr)&&sr>=8000.0&&sr<=384000.0)?static_cast<float>(sr):44100.0f;
        valveDcCoef_=std::exp(-2.0f*kPi*3.0f/sr_);
        tubeHfAlpha_=1.0f-std::exp(-2.0f*kPi*18000.0f/sr_);
        phaseFallbackAlpha_=1.0f-std::exp(-1.0f/(0.001f*sr_));
        phaseWaitLimit_=mpc::clamp(static_cast<int>(sr_*0.020f),1,20000);
        for(auto &c:ch_) c.console.prepare(sr_);
        reset();
    }
    void reset() override {
        for(auto &c:ch_) c.reset();
        detector_=gateDetector_=0.0f; gateGain_=compGain_=1.0f; gateOpen_=true; compRatioCached_=-1.0f;
        // Host state can be restored before prepare/reset. Never seed a smoother
        // with NaN/Inf: one invalid initial value would otherwise poison it forever.
        output_.snap(db(finiteClamp(get(kOutput),-24,24,0)));
        consoleFader_.snap(finiteClamp(get(kConsoleFader),0,1,0.772f));
        preamp_.snap(db(finiteClamp(get(kPreamp),-18,18,0)));
        circuitMix_.snap(finiteClamp(get(kCircuitMix),0,100,0)*0.01f);
        gateThreshDb_.snap(finiteClamp(get(kGateThresh),-80,0,-80));
        gateRangeDb_.snap(finiteClamp(get(kGateRange),0,80,60));
        compThreshDb_.snap(finiteClamp(get(kCompThresh),-40,0,0));
        compRatio_.snap(finiteClamp(get(kCompRatio),1,20,2));
        compMakeupDb_.snap(finiteClamp(get(kCompMakeup),0,24,0));
        drive_.snap(finiteClamp(get(kDrive),0,100,0)*0.01f);
        panL_.snap(finiteClamp(get(kPanL),-100,100,-100)*0.01f);
        panR_.snap(finiteClamp(get(kPanR),-100,100,100)*0.01f);
        outPan_.snap(finiteClamp(get(kOutPan),-100,100,0)*0.01f);
        phaseGain_[0]=phaseTarget_[0]=getBool(kPhaseL)?-1.0f:1.0f;
        phaseGain_[1]=phaseTarget_[1]=getBool(kPhaseR)?-1.0f:1.0f;
        phasePrevious_[0]=phasePrevious_[1]=0.0f;
        phaseHasPrevious_[0]=phaseHasPrevious_[1]=false;
        phaseWait_[0]=phaseWait_[1]=0;
        phaseFallback_[0]=phaseFallback_[1]=false;
        ptLowFreq_.snap(ptLowFrequency(mpc::clamp(getInt(kPtLowFreq),0,3)));
        ptLowBoost_.snap(finiteClamp(get(kPtLowBoost),0,13.5f,0));
        ptLowAtten_.snap(finiteClamp(get(kPtLowAtten),0,17.5f,0));
        ptHighFreq_.snap(ptHighFrequency(mpc::clamp(getInt(kPtHighFreq),0,6)));
        ptHighBoost_.snap(finiteClamp(get(kPtHighBoost),0,18,0));
        ptBandwidth_.snap(finiteClamp(get(kPtBandwidth),0,100,50));
        ptHighAttenFreq_.snap(ptHighAttenFrequency(mpc::clamp(getInt(kPtHighAttenFreq),0,2)));
        ptHighAtten_.snap(finiteClamp(get(kPtHighAtten),0,16,0));
        invertLR_.snap(getBool(kInvertLR)?1.0f:0.0f);
        width_.snap(finiteClamp(get(kWidth),0,200,100)*0.01f);
        correlation_.snap(finiteClamp(get(kCorrelation),0,100,0)*0.01f);
        bassMonoFreq_.snap(finiteClamp(get(kBassMono),20,400,120));
        monoCompatibility_.snap(finiteClamp(static_cast<float>(getInt(kMonoCompatibility)),0,2,0));
        monoThreshold_.snap(finiteClamp(get(kMonoThreshold),0,100,50)*0.01f);
        monoSafety_.snap(1.0f);
        monoCorrPowerL_=monoCorrPowerR_=monoCorrCross_=0.0f;
        bassMonoSideHp_.reset();
        bassMonoSideHp_.pass(sr_,bassMonoFreq_.v,true);
        corrPowerL_=corrPowerR_=corrCross_=0.0f;
        hpFreq_.snap(finiteClamp(get(kHpFreq),20,1000,20));
        lpFreq_.snap(finiteClamp(get(kLpFreq),1000,22000,22000));
        for(int i=0;i<16;++i) graphicEqGain_[i].snap(finiteClamp(get(kGraphicEq01+i),-12.0f,12.0f,0.0f));
        csHpEnable_.snap(getBool(kCsHpEnable)?1.0f:0.0f);
        csHpFreq_.snap(finiteClamp(get(kCsHpFreq),20,11000,20));
        csLpEnable_.snap(getBool(kCsLpEnable)?1.0f:0.0f);
        csLpFreq_.snap(finiteClamp(get(kCsLpFreq),20,22000,22000));
        csHpOrderFade_.snap(1.0f); csLpOrderFade_.snap(1.0f);
    }
    void process(mpc::ProcessContext& ctx) override {
        const int n=mpc::clamp(ctx.numFrames,0,2048);
        if(ctx.outputs==nullptr||ctx.numOutputs<1||ctx.outputs[0]==nullptr) return;
        // Smooth::next() is called once per sample below, so its coefficient must
        // represent one sample of a 10 ms one-pole time constant. Using the whole
        // host block here made the effective smoothing N times too fast (N=frames).
        // Block-rate controls need a coefficient for the elapsed host block.
        const float controlBlockSmooth=1.0f-std::exp(-static_cast<float>(n)/(0.010f*std::fmax(sr_,1.0f)));
        // Per-sample controls use the same 10 ms time constant at every rate.
        const float sampleSmooth=1.0f-std::exp(-1.0f/(0.010f*std::fmax(sr_,1.0f)));
        const float consoleTarget=finiteClamp(get(kConsoleFader),0,1,0.772f);
        const float consoleSmoothing=1.0f-std::exp(-1.0f/(0.0015f*std::fmax(sr_,1.0f)));
        const float preampGain=db(finiteClamp(get(kPreamp),-18,18,0));
        const float outGain=db(finiteClamp(get(kOutput),-24,24,0));
        // Smooth dynamics thresholds in dB (log-amplitude domain), not in
        // linear gain, so a knob move has a perceptually uniform trajectory.
        const float gateThreshold=db(gateThreshDb_.next(finiteClamp(get(kGateThresh),-80,0,-80),controlBlockSmooth));
        const float gateFloor=db(-gateRangeDb_.next(finiteClamp(get(kGateRange),0,80,60),controlBlockSmooth));
        const float compThreshold=db(compThreshDb_.next(finiteClamp(get(kCompThresh),-40,0,0),controlBlockSmooth));
        const float ratio=compRatio_.next(finiteClamp(get(kCompRatio),1,20,2),controlBlockSmooth);
        const float gateAtk=coeff(get(kGateAttack),sr_), gateRel=coeff(get(kGateRelease),sr_);
        const float compAtk=coeff(get(kCompAttack),sr_), compRel=coeff(get(kCompRelease),sr_);
        // Gate timing belongs to its own detector; a short fixed gain slew only
        // removes control discontinuities after the hysteretic state decision.
        const float gateSlew=coeff(2.0f,sr_);
        const float makeup=db(compMakeupDb_.next(finiteClamp(get(kCompMakeup),0,24,0),controlBlockSmooth));
        // Rebuild the static compressor curve only when its ratio changes.
        // A fourth-power overdrive grid gives dense resolution around the knee
        // while spanning >90 dB above threshold without per-sample log/pow work.
        // The audio loop maps overdrive to this grid with two hardware square roots.
        constexpr float kCompCurveMaxOver=8388608.0f;
        constexpr int kCompCurveIntervals=1024;
        if(ratio!=compRatioCached_) {
            const float exponent=-(1.0f-1.0f/ratio);
            for(int i=0;i<=kCompCurveIntervals;++i) {
                const float position=static_cast<float>(i)*(1.0f/static_cast<float>(kCompCurveIntervals));
                const float position2=position*position;
                const float over=1.0f+(kCompCurveMaxOver-1.0f)*position2*position2;
                compCurve_[i]=std::pow(over,exponent);
            }
            compRatioCached_=ratio;
        }
        const float drive=drive_.next(finiteClamp(get(kDrive)*0.01f,0,1,0),controlBlockSmooth);
        const float cmix=finiteClamp(get(kCircuitMix)*0.01f,0,1,0);

        const bool phaseTargetL=getBool(kPhaseL), phaseTargetR=getBool(kPhaseR);
        const float hpTarget=finiteClamp(get(kHpFreq),20,1000,20);
        const float lpTarget=finiteClamp(get(kLpFreq),1000,22000,22000);
        const float csHpTarget=finiteClamp(get(kCsHpFreq),20,11000,20);
        const float csLpTarget=finiteClamp(get(kCsLpFreq),20,22000,22000);
        const int csHpOrder=mpc::clamp(getInt(kCsHpOrder)+1,1,6);
        const int csLpOrder=mpc::clamp(getInt(kCsLpOrder)+1,1,6);
        const float ptLfF=smoothLog(ptLowFreq_.v,ptLowFrequency(getInt(kPtLowFreq)),controlBlockSmooth); ptLowFreq_.snap(ptLfF);
        const float ptLfBoost=ptLowBoost_.next(finiteClamp(get(kPtLowBoost),0,13.5f,0),controlBlockSmooth), ptLfAtten=ptLowAtten_.next(finiteClamp(get(kPtLowAtten),0,17.5f,0),controlBlockSmooth);
        const float ptHfF=smoothLog(ptHighFreq_.v,ptHighFrequency(getInt(kPtHighFreq)),controlBlockSmooth); ptHighFreq_.snap(ptHfF);
        const float ptHfBoost=ptHighBoost_.next(finiteClamp(get(kPtHighBoost),0,18,0),controlBlockSmooth), ptBandwidth=ptBandwidth_.next(finiteClamp(get(kPtBandwidth),0,100,50),controlBlockSmooth);
        const float ptHfAttF=smoothLog(ptHighAttenFreq_.v,ptHighAttenFrequency(getInt(kPtHighAttenFreq)),controlBlockSmooth); ptHighAttenFreq_.snap(ptHfAttF);
        const float ptHfAtten=ptHighAtten_.next(finiteClamp(get(kPtHighAtten),0,16,0),controlBlockSmooth);
        const float correlationAlpha=1.0f-std::exp(-1.0f/(0.050f*sr_));
        const float monoCorrelationAlpha=correlationAlpha;
        const float ptLowBoostAlpha=1.0f-std::exp(-2.0f*kPi*ptLfF/sr_);
        const float ptLowAttenAlpha=1.0f-std::exp(-2.0f*kPi*ptLfF*1.41421356f/sr_);
        const float ptHighAttenAlpha=1.0f-std::exp(-2.0f*kPi*ptHfAttF/sr_);
        const float ptLowBoostGain=db(ptLfBoost), ptLowAttenGain=db(-ptLfAtten), ptHighAttenGain=db(-ptHfAtten);
        const float ptInteractionDb=(ptLfBoost>0 && ptLfAtten>0)?5.0f*(1.0f-std::exp(-std::fmin(ptLfBoost,ptLfAtten)/5.0f)):0.0f;
        const float ptHfQ=1.60f-1.15f*(ptBandwidth*0.01f);
        for(int c=0;c<2;++c) {
            ch_[c].ptHighBoost.resonantBoost(sr_,ptHfF,ptHfQ,ptHfBoost*(0.5f+0.5f*(1.0f-ptBandwidth*0.01f)));
            ch_[c].ptLfInteraction.bell(sr_,finiteClamp(ptLfF*8.0f,100.0f,sr_*0.45f,800.0f),0.70f,-ptInteractionDb);
        }
        // The shared HP/LP section uses the same 10 ms logarithmic glide and
        // 64-frame coefficient cadence in both Stratt plugins.
        for(int blockStart=0;blockStart<n;blockStart+=64) {
            const int blockFrames=mpc::clamp(n-blockStart,1,64);
            const float filterSmooth=1.0f-std::exp(-static_cast<float>(blockFrames)/(0.010f*std::fmax(sr_,1.0f)));
            // Refresh stereo-image controls every 64 frames, not once per host callback.
            // This keeps pan, width and M/S inversion automation smooth at large block sizes.
            const float inL=panL_.next(finiteClamp(get(kPanL)*0.01f,-1,1,-1),filterSmooth);
            const float inR=panR_.next(finiteClamp(get(kPanR)*0.01f,-1,1,1),filterSmooth);
            const float ilL=panL(inL), ilR=panR(inL), irL=panL(inR), irR=panR(inR);
            const float op=outPan_.next(finiteClamp(get(kOutPan)*0.01f,-1,1,0),filterSmooth);
            const float oL=op>0?std::cos(op*kHalfPi*0.5f):1.0f;
            const float oR=op<0?std::cos(-op*kHalfPi*0.5f):1.0f;
            const float invertLR=invertLR_.next(getBool(kInvertLR)?1.0f:0.0f,filterSmooth);
            const float swapSideGain=std::cos(invertLR*kPi);
            const float width=width_.next(finiteClamp(get(kWidth),0,200,100)*0.01f,filterSmooth);
            const float correlation=correlation_.next(finiteClamp(get(kCorrelation),0,100,0)*0.01f,filterSmooth);
            const float monoMode=monoCompatibility_.next(finiteClamp(static_cast<float>(getInt(kMonoCompatibility)),0,2,0),filterSmooth);
            const float monoThreshold=monoThreshold_.next(finiteClamp(get(kMonoThreshold),0,100,50)*0.01f,filterSmooth);
            const float bassFreq=smoothLog(bassMonoFreq_.v,finiteClamp(get(kBassMono),20,400,120),filterSmooth);
            bassMonoFreq_.snap(bassFreq);
            bassMonoSideHp_.pass(sr_,bassFreq,true);
            float graphicEqGains[16];
            for(int band=0;band<16;++band)
                graphicEqGains[band]=graphicEqGain_[band].next(finiteClamp(get(kGraphicEq01+band),-12.0f,12.0f,0.0f),filterSmooth);
            const float hp=smoothLog(hpFreq_.v,hpTarget,filterSmooth); hpFreq_.snap(hp);
            const float lp=smoothLog(lpFreq_.v,lpTarget,filterSmooth); lpFreq_.snap(lp);
            const float csHp=smoothLog(csHpFreq_.v,csHpTarget,filterSmooth); csHpFreq_.snap(csHp);
            const float csLp=smoothLog(csLpFreq_.v,csLpTarget,filterSmooth); csLpFreq_.snap(csLp);
            const bool csHpOrderChanged=ch_[0].csHp.order()!=csHpOrder;
            const bool csLpOrderChanged=ch_[0].csLp.order()!=csLpOrder;
            for(int c=0;c<2;++c) {
                ch_[c].hp.setCutoff(sr_,hp); ch_[c].lp.setCutoff(sr_,lp);
                for(int band=0;band<16;++band) ch_[c].graphic[band].bell(sr_,kGraphicEqFrequencies[band],kGraphicEqQ,graphicEqGains[band]);
                if(csHpOrderChanged) ch_[c].csHpOld=ch_[c].csHp;
                if(csLpOrderChanged) ch_[c].csLpOld=ch_[c].csLp;
                ch_[c].csHp.set(sr_,csHp,csHpOrder); ch_[c].csLp.set(sr_,csLp,csLpOrder);
            }
            if(csHpOrderChanged) csHpOrderFade_.snap(0.0f);
            if(csLpOrderChanged) csLpOrderFade_.snap(0.0f);
            const int blockEnd=blockStart+blockFrames;
            for(int i=blockStart;i<blockEnd;++i) {
            const float og=output_.next(outGain,sampleSmooth);
            const float pg=preamp_.next(preampGain,sampleSmooth);
            const float csHpMix=csHpEnable_.next(getBool(kCsHpEnable)?1.0f:0.0f,sampleSmooth);
            const float csLpMix=csLpEnable_.next(getBool(kCsLpEnable)?1.0f:0.0f,sampleSmooth);
            const float csHpOrderMix=csHpOrderFade_.next(1.0f,sampleSmooth);
            const float csLpOrderMix=csLpOrderFade_.next(1.0f,sampleSmooth);
            const float mix=circuitMix_.next(cmix,sampleSmooth);
            const float consoleFader=consoleFader_.next(consoleTarget,consoleSmoothing);
            const float l=(ctx.inputs&&ctx.numInputs>0&&ctx.inputs[0])?ctx.inputs[0][i]:0.0f;
            const float r=(ctx.inputs&&ctx.numInputs>1&&ctx.inputs[1])?ctx.inputs[1][i]:0.0f;
            const float consoleL=ch_[0].console.process(finiteClamp(l,-16,16,0),consoleFader);
            const float consoleR=ch_[1].console.process(finiteClamp(r,-16,16,0),consoleFader);
            const float a=applyPolarity(consoleL*pg,0,phaseTargetL);
            const float b=applyPolarity(consoleR*pg,1,phaseTargetR);
            float xL=(a*ilL+b*irL), xR=(a*ilR+b*irR);
            // Exchange channel positions in the M/S domain: keep Mid invariant
            // and smoothly reverse Side. This preserves mono level throughout the
            // transition (unlike an equal-power L/R crossfade, which adds +3 dB
            // to identical dual-mono signals at its midpoint).
            const float swapMid=0.5f*(xL+xR);
            const float swapSide=0.5f*(xL-xR)*swapSideGain;
            xL=swapMid+swapSide;
            xR=swapMid-swapSide;
            // 50 ms normalized correlation estimate; only sustained negative rho
            // can trigger the optional side-channel safeguard.
            corrPowerL_+=correlationAlpha*(xL*xL-corrPowerL_);
            corrPowerR_+=correlationAlpha*(xR*xR-corrPowerR_);
            corrCross_+=correlationAlpha*(xL*xR-corrCross_);
            const float corrDenom=std::sqrt(std::fmax(corrPowerL_*corrPowerR_,1.0e-20f));
            // Do not interpret near-silence / quantization residue as a meaningful stereo relationship.
            // -90 dBFS RMS corresponds to a mean-square floor of 1e-9 for normalized float audio.
            const bool correlationActive=corrPowerL_>1.0e-9f && corrPowerR_>1.0e-9f;
            const float rho=correlationActive?finiteClamp(corrCross_/corrDenom,-1.0f,1.0f,0.0f):0.0f;
            const float sideSafety=1.0f-correlation*std::fmax(0.0f,-rho);
            // M/S matrix: Mid is the mono fold-down; Width scales existing Side only.
            const float mid=0.5f*(xL+xR);
            const float side=0.5f*(xL-xR);
            const float bassMonoSide=bassMonoSideHp_.run(side);
            const float shapedSide=bassMonoSide*width*sideSafety;
            xL=mid+shapedSide;
            xR=mid-shapedSide;
            float *samples[2]={&xL,&xR};
            // Clean filtering and EQ precede the linked dynamics detector.
            for(int ch=0;ch<2;++ch) {
                Channel &s=ch_[ch]; float x=*samples[ch];
                x=s.hp.process(x); x=s.lp.process(x);
                float csHpWet=s.csHp.process(x);
                if(csHpOrderMix<0.9999f) {
                    const float old=s.csHpOld.process(x);
                    csHpWet=old+(csHpWet-old)*csHpOrderMix;
                }
                x += (csHpWet-x)*csHpMix;
                float csLpWet=s.csLp.process(x);
                if(csLpOrderMix<0.9999f) {
                    const float old=s.csLpOld.process(x);
                    csLpWet=old+(csLpWet-old)*csLpOrderMix;
                }
                x += (csLpWet-x)*csLpMix;
                s.ptLowBoostState+=ptLowBoostAlpha*(x-s.ptLowBoostState); x+=(ptLowBoostGain-1.0f)*s.ptLowBoostState;
                s.ptLowAttenState+=ptLowAttenAlpha*(x-s.ptLowAttenState); x+=(ptLowAttenGain-1.0f)*s.ptLowAttenState;
                x=s.ptLfInteraction.run(x); x=s.ptHighBoost.run(x);
                s.ptHighAttenState+=ptHighAttenAlpha*(x-s.ptHighAttenState); x=s.ptHighAttenState+ptHighAttenGain*(x-s.ptHighAttenState);
                for(int band=0;band<16;++band) x=s.graphic[band].run(x);
                *samples[ch]=x;
            }
            // One linked-stereo dynamics detector/gain computer: both channels
            // receive exactly the same gain reduction and gate envelope.
            const float peak=std::fmax(std::fabs(xL),std::fabs(xR));
            const float envCoef=peak>detector_?compAtk:compRel;
            detector_=envCoef*detector_+(1.0f-envCoef)*peak;
            const float over=detector_/std::fmax(compThreshold,1.0e-9f);
            // Conventional hard-knee compressor law in the amplitude domain.
            // The fourth-power grid resolves the knee accurately and keeps the
            // ratio law valid up to 8,388,608x threshold (about +138.5 dB).
            const float normalizedOver=finiteClamp((over-1.0f)/(kCompCurveMaxOver-1.0f),0.0f,1.0f,0.0f);
            const float curvePos=std::sqrt(std::sqrt(normalizedOver))*static_cast<float>(kCompCurveIntervals);
            const int curveIndex=static_cast<int>(curvePos);
            const float curveFrac=curvePos-static_cast<float>(curveIndex);
            const float targetComp=compCurve_[curveIndex]+(compCurve_[curveIndex<kCompCurveIntervals?curveIndex+1:kCompCurveIntervals]-compCurve_[curveIndex])*curveFrac;
            // The compressor envelope already supplies the specified attack/release.
            // Applying a second identical smoother to gain would double its timing.
            compGain_=targetComp;
            // Independent gate envelope: gate timing must not follow compressor timing.
            const float gateEnvCoef=peak>gateDetector_?gateAtk:gateRel;
            gateDetector_=gateEnvCoef*gateDetector_+(1.0f-gateEnvCoef)*peak;
            // 3 dB hysteresis prevents gate chatter around its opening threshold.
            const float gateClose=gateThreshold*db(-3.0f);
            if(gateOpen_ ? gateDetector_<gateClose : gateDetector_>gateThreshold) gateOpen_=!gateOpen_;
            const float gateTarget=gateOpen_?1.0f:gateFloor;
            gateGain_=gateSlew*gateGain_+(1.0f-gateSlew)*gateTarget;
            for(int ch=0;ch<2;++ch) {
                *samples[ch]=*samples[ch]*gateGain_*compGain_*makeup;
                const float clean=*samples[ch];
                // User-controlled drive colour remains a separate parallel stage.
                // Its Mix=0 setting is exactly the clean dynamics signal.
                const float driveWet=circuit(clean,drive);
                *samples[ch]=clean+(driveWet-clean)*mix;
            }
            // Mono Compatibility is deliberately the final stereo-image stage before Valve.
            // Estimate correlation after all channel processing, then attenuate only Side.
            monoCorrPowerL_+=monoCorrelationAlpha*(xL*xL-monoCorrPowerL_);
            monoCorrPowerR_+=monoCorrelationAlpha*(xR*xR-monoCorrPowerR_);
            monoCorrCross_+=monoCorrelationAlpha*(xL*xR-monoCorrCross_);
            const bool monoCorrActive=monoCorrPowerL_>1.0e-9f && monoCorrPowerR_>1.0e-9f;
            const float monoDenom=std::sqrt(std::fmax(monoCorrPowerL_*monoCorrPowerR_,1.0e-20f));
            const float monoRho=monoCorrActive?finiteClamp(monoCorrCross_/monoDenom,-1.0f,1.0f,0.0f):0.0f;
            float monoSafetyTarget=1.0f;
            if(monoMode>=1.5f) monoSafetyTarget=0.0f;
            else if(monoMode>=0.5f && monoCorrActive && monoRho <= -monoThreshold) {
                const float span=std::fmax(1.0f-monoThreshold,1.0e-4f);
                monoSafetyTarget=finiteClamp((1.0f+monoRho)/span,0.0f,1.0f,1.0f);
            }
            const float monoSafety=monoSafety_.next(monoSafetyTarget,sampleSmooth);
            const float monoMid=0.5f*(xL+xR);
            const float monoSide=0.5f*(xL-xR)*monoSafety;
            xL=monoMid+monoSide;
            xR=monoMid-monoSide;
            // Permanent, invisible valve-amplifier coloration: independent of
            // Circuit Mix, after all tone/dynamics stages and before output gain.
            // The same ADAA triode-inspired stage is used by StrattPultek.
            for(int ch=0;ch<2;++ch)
                *samples[ch]=ch_[ch].tubeStage(*samples[ch],valveDcCoef_,tubeHfAlpha_);
            // Output balance is applied before the final master Output gain.
            if(ctx.numOutputs==1) ctx.outputs[0][i]=0.5f*(xL+xR)*og;
            else {
                if(ctx.outputs[0]) ctx.outputs[0][i]=xL*oL*og;
                if(ctx.outputs[1]) ctx.outputs[1][i]=xR*oR*og;
                for(int c=2;c<ctx.numOutputs;++c) if(ctx.outputs[c]) ctx.outputs[c][i]=0.0f;
            }
            }
        }
    }
private:
    static mpc::PluginInfo makeInfo() {
        mpc::PluginInfo x; x.name="StrattTrack"; x.vendor="Stratt";
        x.uniqueId=MPC_FOURCC('S','4','X','3'); x.version=1400;
        x.isInstrument=false; x.numInputs=2; x.numOutputs=2; x.tailSamples=1; return x;
    }
    float applyPolarity(float x,int channel,bool invert) {
        const float desired=invert?-1.0f:1.0f;
        if(desired!=phaseTarget_[channel]) {
            phaseTarget_[channel]=desired;
            phaseWait_[channel]=0;
            phaseFallback_[channel]=false;
        }
        if(phaseGain_[channel]!=desired) {
            // A polarity switch is discrete, not a continuously swept phase angle:
            // commit at the first zero crossing to avoid a 10 ms amplitude null.
            if(!phaseFallback_[channel]) {
                if((phaseHasPrevious_[channel] && phasePrevious_[channel]*x<=0.0f) || std::fabs(x)<1.0e-7f) {
                    phaseGain_[channel]=desired;
                    phaseWait_[channel]=0;
                } else if(++phaseWait_[channel]>=phaseWaitLimit_) {
                    // DC or near-DC material may never cross zero; use a 1 ms
                    // one-pole polarity glide as a bounded click-safe fallback.
                    phaseFallback_[channel]=true;
                }
            }
            if(phaseFallback_[channel]) {
                phaseGain_[channel]+=phaseFallbackAlpha_*(desired-phaseGain_[channel]);
                if(std::fabs(desired-phaseGain_[channel])<1.0e-4f) {
                    phaseGain_[channel]=desired;
                    phaseFallback_[channel]=false;
                    phaseWait_[channel]=0;
                }
            }
        }
        phasePrevious_[channel]=x;
        phaseHasPrevious_[channel]=true;
        return x*phaseGain_[channel];
    }
    Channel ch_[2]; float sr_=44100;
    Smooth output_,preamp_,circuitMix_,consoleFader_;
    Smooth csHpEnable_,csHpFreq_,csLpEnable_,csLpFreq_,csHpOrderFade_,csLpOrderFade_;
    Smooth gateThreshDb_,gateRangeDb_,compThreshDb_,compRatio_,compMakeupDb_,drive_;
    Smooth panL_,panR_,outPan_;
    float phaseGain_[2]={1.0f,1.0f}, phaseTarget_[2]={1.0f,1.0f}, phasePrevious_[2]={0.0f,0.0f};
    bool phaseHasPrevious_[2]={false,false};
    int phaseWait_[2]={0,0};
    bool phaseFallback_[2]={false,false};
    Smooth invertLR_,width_,correlation_,bassMonoFreq_;
    Smooth monoCompatibility_,monoThreshold_,monoSafety_;
    float monoCorrPowerL_=0.0f, monoCorrPowerR_=0.0f, monoCorrCross_=0.0f;
    Biquad bassMonoSideHp_;
    float corrPowerL_=0.0f, corrPowerR_=0.0f, corrCross_=0.0f;
    Smooth hpFreq_,lpFreq_;
    Smooth graphicEqGain_[16];
    Smooth ptLowFreq_,ptLowBoost_,ptLowAtten_,ptHighFreq_,ptHighBoost_,ptBandwidth_,ptHighAttenFreq_,ptHighAtten_;
    // Linked dynamics state is separate from per-channel filter/colour state.
    float detector_=0.0f, gateDetector_=0.0f, gateGain_=1.0f, compGain_=1.0f;
    bool gateOpen_=true;
    float compCurve_[1025]{};
    float compRatioCached_=-1.0f;
    float valveDcCoef_=0.999572f;
    float tubeHfAlpha_=0.923f;
    float phaseFallbackAlpha_=0.022420575f;
    int phaseWaitLimit_=882;
};
MPC_PLUGIN_ENTRY(StrattTrack)
