// StrattTrack — transparent SSL-style stereo channel strip for MPC / Force.
// SPDX-License-Identifier: MIT
// Signal flow: Airwindows Console7 input stage -> input trim -> preamp -> L/R polarity -> dual input pan ->
// HP/LP -> Pulse Techniques EQP-1A curve model -> four-band EQ -> gate ->
// compressor -> user-controlled drive colour (parallel blend) -> permanent
// invisible ADAA valve stage -> output trim -> output balance -> fader.
//
// All clean stages use linear gain and stable cookbook biquads. The Drive/Circuit
// control affects only its own parallel colour path; the fixed valve stage is
// separate, permanently active, and has no parameter or UI mapping.
// No allocation, locks, I/O or per-sample transcendental functions occur in process().

#include "mpc_plugin.h"
#include "dsp/two_pole_filter.h"
#include "dsp/airwindows_console7.h"
#include <cmath>

namespace {
enum P {
    kOutput=0, kInput, kPreamp, kPhaseL, kPhaseR, kPanL, kPanR,
    kHpFreq, kLpFreq,
    kLfFreq, kLfGain, kLmFreq, kLmGain, kLmQ,
    kHmFreq, kHmGain, kHmQ, kHfFreq, kHfGain,
    kGateThresh, kGateRange, kGateAttack, kGateRelease,
    kCompThresh, kCompRatio, kCompAttack, kCompRelease, kCompMakeup,
    kDrive, kCircuitMix, kOutPan, kFader,
    kPtLowFreq, kPtLowBoost, kPtLowAtten, kPtHighFreq,
    kPtHighBoost, kPtBandwidth, kPtHighAttenFreq, kPtHighAtten, kConsoleFader, kCount
};
constexpr float kPi=3.14159265358979323846f;
constexpr float kHalfPi=1.57079632679489661923f;
inline float db(float x) { return std::pow(10.0f, x*0.05f); }
inline float finiteClamp(float x,float lo,float hi,float fallback) {
    return std::isfinite(x) ? mpc::clamp(x,lo,hi) : fallback;
}
struct Smooth {
    float v=0.0f;
    float next(float target,float c=0.002f) {
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

// Five-point Gauss-Legendre integrates polynomials through degree 9 exactly.
// F + eta*F^2 is degree 8 in each region. Split at every knee so each
// quadrature interval contains one polynomial branch and avoid subtracting
// nearly equal global primitives when a tiny interval straddles a knee.
static inline double integrateShortCrossingInterval(double lo, double hi, double eta)
{
    constexpr double nodes[2] = {0.53846931010568309104, 0.90617984593866399280};
    constexpr double weights[3] = {0.56888888888888888889,
                                   0.47862867049936646804,
                                   0.23692688505618908751};
    constexpr double boundaries[4] = {-2.0, -1.0, 1.0, 2.0};
    double integral = 0.0;
    double start = lo;

    for (double boundary : boundaries)
    {
        if (boundary <= start || boundary >= hi) continue;
        const double midpoint = 0.5 * (start + boundary);
        const double halfWidth = 0.5 * (boundary - start);
        const double pair0 = softClipTransfer(midpoint - nodes[0] * halfWidth, eta)
                           + softClipTransfer(midpoint + nodes[0] * halfWidth, eta);
        const double pair1 = softClipTransfer(midpoint - nodes[1] * halfWidth, eta)
                           + softClipTransfer(midpoint + nodes[1] * halfWidth, eta);
        integral += halfWidth * (weights[0] * softClipTransfer(midpoint, eta)
                               + weights[1] * pair0 + weights[2] * pair1);
        start = boundary;
    }

    const double midpoint = 0.5 * (start + hi);
    const double halfWidth = 0.5 * (hi - start);
    const double pair0 = softClipTransfer(midpoint - nodes[0] * halfWidth, eta)
                       + softClipTransfer(midpoint + nodes[0] * halfWidth, eta);
    const double pair1 = softClipTransfer(midpoint - nodes[1] * halfWidth, eta)
                       + softClipTransfer(midpoint + nodes[1] * halfWidth, eta);
    integral += halfWidth * (weights[0] * softClipTransfer(midpoint, eta)
                           + weights[1] * pair0 + weights[2] * pair1);
    return integral;
}

// First-order antiderivative antialiasing for F_a(x)=F(x)+eta*F(x)^2.
// Each ADAA path seeds its previous sample from the first live input after reset,
// avoiding an artificial interval from digital silence to a non-zero first sample.
// For very short intervals wholly inside one polynomial branch, evaluate the
// interval average with its symmetric Taylor expansion. This avoids subtracting
// nearly equal global primitives. The fourth-order term is exact through O(d^4);
// the remaining O(d^6) error is negligible at the 1e-4 relative interval limit.
// Intervals touching a knee still use the exact piecewise primitive difference.
static inline float adaaSoftClip(float x, float previousX, float asymmetry)
{
    const double xd = static_cast<double>(x);
    const double pd = static_cast<double>(previousX);
    const double delta = xd - pd;
    const double ad = std::fabs(delta);
    const double scale = std::fmax(1.0, std::fmax(std::fabs(xd), std::fabs(pd)));
    const double eta = static_cast<double>(asymmetry);
    const double midpoint = 0.5 * (xd + pd);

    if (ad <= 1.0e-4 * scale)
    {
        const double ax = std::fabs(xd);
        const double ap = std::fabs(pd);
        const double d2 = delta * delta;
        const double d4 = d2 * d2;

        // In the linear branch F_a(x)=x+eta*x^2, so its interval
        // average is available exactly (the second derivative is 2*eta).
        if (ax <= 1.0 && ap <= 1.0)
            return static_cast<float>(midpoint + eta * (midpoint * midpoint + d2 / 12.0));

        // The plateau is constant as long as the interval stays on one side.
        if (ax >= 2.0 && ap >= 2.0 && std::signbit(xd) == std::signbit(pd))
        {
            const double y = 1.5 * (std::signbit(xd) ? -1.0 : 1.0);
            return static_cast<float>(y + eta * y * y);
        }

        // In the knee, use f(mid) + f''(mid)d^2/24 + f''''(mid)d^4/1920.
        // The polynomial is degree four; its asymmetric square is degree eight.
        if (ax > 1.0 && ax < 2.0 && ap > 1.0 && ap < 2.0
            && std::signbit(xd) == std::signbit(pd))
        {
            const double sign = std::signbit(midpoint) ? -1.0 : 1.0;
            const double t = std::fabs(midpoint) - 1.0;
            const double t2 = t * t;
            const double p = 1.0 + t - t * t2 + 0.5 * t2 * t2;
            const double p1 = 1.0 - 3.0 * t2 + 2.0 * t2 * t;
            const double p2 = -6.0 * t + 6.0 * t2;
            const double p3 = -6.0 + 12.0 * t;
            constexpr double p4 = 12.0;
            const double f = sign * p;
            const double f2 = sign * p2;
            const double f4 = sign * p4;
            const double fa = f + eta * p * p;
            const double fa2 = f2 + eta * (2.0 * p1 * p1 + 2.0 * p * p2);
            const double fa4 = f4 + eta * (2.0 * p * p4 + 8.0 * p1 * p3 + 6.0 * p2 * p2);
            return static_cast<float>(fa + fa2 * d2 / 24.0 + fa4 * d4 / 1920.0);
        }
    }

    if (ad <= 1.0e-4 * scale)
    {
        const double lo = std::fmin(xd, pd);
        const double hi = std::fmax(xd, pd);
        return static_cast<float>(integrateShortCrossingInterval(lo, hi, eta) / (hi - lo));
    }

    const double currentIntegral = softClipIntegral(xd)
                                 + eta * softClipSquareIntegral(xd);
    const double previousIntegral = softClipIntegral(pd)
                                  + eta * softClipSquareIntegral(pd);
    return static_cast<float>((currentIntegral - previousIntegral) / delta);
}


struct Channel {
    mpc::dsp::TwoPoleButterworth hp{mpc::dsp::TwoPoleButterworth::Mode::Highpass};
    mpc::dsp::TwoPoleButterworth lp{mpc::dsp::TwoPoleButterworth::Mode::Lowpass};
    Biquad lf,lm,hm,hf,ptHighBoost,ptLfInteraction;
    stratt::dsp::Console7Channel console;
    float ptLowBoostState=0,ptLowAttenState=0,ptHighAttenState=0;
    float previousTubeInput=0.0f;
    bool hasPreviousTubeInput=false;
    float previousTubeDcInput=0.0f, previousTubeDcOutput=0.0f;
    void reset(){console.reset();hp.reset();lp.reset();lf.reset();lm.reset();hm.reset();hf.reset();
        ptHighBoost.reset();ptLfInteraction.reset();
        ptLowBoostState=ptLowAttenState=ptHighAttenState=0;
        previousTubeInput=previousTubeDcInput=previousTubeDcOutput=0.0f;
        hasPreviousTubeInput=false;}
    float tubeStage(float x,float pole) {
        constexpr float tubeDrive=1.15f;
        constexpr float tubeAsymmetry=0.045f;
        const float driven=x*tubeDrive;
        const float previous=hasPreviousTubeInput?previousTubeInput:driven;
        const float shaped=adaaSoftClip(driven,previous,tubeAsymmetry)/tubeDrive;
        previousTubeInput=driven;
        hasPreviousTubeInput=true;
        const float y=shaped-previousTubeDcInput+pole*previousTubeDcOutput;
        previousTubeDcInput=shaped;
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
        addProgram("Clean", {0,0,0,0,0,-100,100,20,22000,100,0,400,0,1,2500,0,1,12000,0,-80,60,1,120,0,2,10,100,0,0,0,0,0,2,0,0,2,0,50,1,0,0.772f});
        addProgram("Vocal", {0,0,0,0,0,-100,100,80,18000,120,1,350,-2,1.2f,3200,2,1.1f,12000,1,-45,35,1,100,-18,3,8,90,2,12,25,0,0,2,0,0,2,0,50,1,0,0.772f});
    }
    void prepare(double sr,int) override {
        sr_=(std::isfinite(sr)&&sr>=8000.0&&sr<=384000.0)?static_cast<float>(sr):44100.0f;
        valveDcCoef_=std::exp(-2.0f*kPi*3.0f/sr_);
        for(auto &c:ch_) c.console.prepare(sr_);
        reset();
    }
    void reset() override {
        for(auto &c:ch_) c.reset();
        detector_=gateDetector_=0.0f; gateGain_=compGain_=1.0f; gateOpen_=true; compRatioCached_=-1.0f;
        // Host state can be restored before prepare/reset. Never seed a smoother
        // with NaN/Inf: one invalid initial value would otherwise poison it forever.
        output_.snap(db(finiteClamp(get(kOutput),-24,24,0)));
        input_.snap(db(finiteClamp(get(kInput),-24,24,0)));
        consoleFader_.snap(finiteClamp(get(kConsoleFader),0,1,0.772f));
        preamp_.snap(db(finiteClamp(get(kPreamp),-18,18,0)));
        fader_.snap(db(finiteClamp(get(kFader),-60,12,0)));
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
        phaseL_.snap(getBool(kPhaseL)?-1.0f:1.0f);
        phaseR_.snap(getBool(kPhaseR)?-1.0f:1.0f);
        ptLowFreq_.snap(ptLowFrequency(mpc::clamp(getInt(kPtLowFreq),0,3)));
        ptLowBoost_.snap(finiteClamp(get(kPtLowBoost),0,13.5f,0));
        ptLowAtten_.snap(finiteClamp(get(kPtLowAtten),0,17.5f,0));
        ptHighFreq_.snap(ptHighFrequency(mpc::clamp(getInt(kPtHighFreq),0,6)));
        ptHighBoost_.snap(finiteClamp(get(kPtHighBoost),0,18,0));
        ptBandwidth_.snap(finiteClamp(get(kPtBandwidth),0,100,50));
        ptHighAttenFreq_.snap(ptHighAttenFrequency(mpc::clamp(getInt(kPtHighAttenFreq),0,2)));
        ptHighAtten_.snap(finiteClamp(get(kPtHighAtten),0,16,0));
        hpFreq_.snap(finiteClamp(get(kHpFreq),20,1000,20));
        lpFreq_.snap(finiteClamp(get(kLpFreq),1000,22000,22000));
        lfFreq_.snap(finiteClamp(get(kLfFreq),30,450,100));
        lfGain_.snap(finiteClamp(get(kLfGain),-15,15,0));
        lmFreq_.snap(finiteClamp(get(kLmFreq),100,2000,400));
        lmGain_.snap(finiteClamp(get(kLmGain),-15,15,0));
        lmQ_.snap(finiteClamp(get(kLmQ),0.4f,4.0f,1.0f));
        hmFreq_.snap(finiteClamp(get(kHmFreq),500,8000,2500));
        hmGain_.snap(finiteClamp(get(kHmGain),-15,15,0));
        hmQ_.snap(finiteClamp(get(kHmQ),0.4f,4.0f,1.0f));
        hfFreq_.snap(finiteClamp(get(kHfFreq),2000,20000,12000));
        hfGain_.snap(finiteClamp(get(kHfGain),-15,15,0));
    }
    void process(mpc::ProcessContext& ctx) override {
        const int n=mpc::clamp(ctx.numFrames,0,2048);
        if(ctx.outputs==nullptr||ctx.numOutputs<1||ctx.outputs[0]==nullptr) return;
        // Smooth::next() is called once per sample below, so its coefficient must
        // represent one sample of a 10 ms one-pole time constant. Using the whole
        // host block here made the effective smoothing N times too fast (N=frames).
        const float controlSmooth=1.0f-std::exp(-1.0f/(0.010f*std::fmax(sr_,1.0f)));
        // Block-rate controls need a coefficient for the elapsed host block.
        const float controlBlockSmooth=1.0f-std::exp(-static_cast<float>(n)/(0.010f*std::fmax(sr_,1.0f)));
        const float consoleTarget=finiteClamp(get(kConsoleFader),0,1,0.772f);
        const float consoleSmoothing=1.0f-std::exp(-1.0f/(0.0015f*std::fmax(sr_,1.0f)));
        const float inGain=db(finiteClamp(get(kInput),-24,24,0));
        const float preampGain=db(finiteClamp(get(kPreamp),-18,18,0));
        const float outGain=db(finiteClamp(get(kOutput),-24,24,0));
        const float faderGain=db(finiteClamp(get(kFader),-60,12,0));
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
        // Rebuild the static compressor curve only when its ratio changes. The
        // audio loop then uses a bounded linear lookup/interpolation (no pow/log).
        if(ratio!=compRatioCached_) {
            const float exponent=-(1.0f-1.0f/ratio);
            for(int i=0;i<=256;++i) {
                const float over=1.0f+(2047.0f/256.0f)*static_cast<float>(i);
                compCurve_[i]=std::pow(over,exponent);
            }
            compRatioCached_=ratio;
        }
        const float drive=drive_.next(finiteClamp(get(kDrive)*0.01f,0,1,0),controlBlockSmooth);
        const float cmix=finiteClamp(get(kCircuitMix)*0.01f,0,1,0);
        const float inL=panL_.next(finiteClamp(get(kPanL)*0.01f,-1,1,-1),controlBlockSmooth), inR=panR_.next(finiteClamp(get(kPanR)*0.01f,-1,1,1),controlBlockSmooth);
        const float ilL=panL(inL), ilR=panR(inL), irL=panL(inR), irR=panR(inR);
        const float op=outPan_.next(finiteClamp(get(kOutPan)*0.01f,-1,1,0),controlBlockSmooth);
        const float oL=op>0?std::cos(op*kHalfPi*0.5f):1.0f, oR=op<0?std::cos(-op*kHalfPi*0.5f):1.0f;
        const float phaseL=phaseL_.next(getBool(kPhaseL)?-1.0f:1.0f,controlBlockSmooth), phaseR=phaseR_.next(getBool(kPhaseR)?-1.0f:1.0f,controlBlockSmooth);
        const float hpTarget=finiteClamp(get(kHpFreq),20,1000,20);
        const float lpTarget=finiteClamp(get(kLpFreq),1000,22000,22000);
        const float lfF=lfFreq_.next(finiteClamp(get(kLfFreq),30,450,100),controlBlockSmooth), lfG=lfGain_.next(finiteClamp(get(kLfGain),-15,15,0),controlBlockSmooth);
        const float lmF=lmFreq_.next(finiteClamp(get(kLmFreq),100,2000,400),controlBlockSmooth), lmG=lmGain_.next(finiteClamp(get(kLmGain),-15,15,0),controlBlockSmooth), lmQv=lmQ_.next(finiteClamp(get(kLmQ),0.4f,4,1),controlBlockSmooth);
        const float hmF=hmFreq_.next(finiteClamp(get(kHmFreq),500,8000,2500),controlBlockSmooth), hmG=hmGain_.next(finiteClamp(get(kHmGain),-15,15,0),controlBlockSmooth), hmQv=hmQ_.next(finiteClamp(get(kHmQ),0.4f,4,1),controlBlockSmooth);
        const float hfF=hfFreq_.next(finiteClamp(get(kHfFreq),2000,20000,12000),controlBlockSmooth), hfG=hfGain_.next(finiteClamp(get(kHfGain),-15,15,0),controlBlockSmooth);
        const float ptLfF=smoothLog(ptLowFreq_.v,ptLowFrequency(getInt(kPtLowFreq)),controlBlockSmooth); ptLowFreq_.snap(ptLfF);
        const float ptLfBoost=ptLowBoost_.next(finiteClamp(get(kPtLowBoost),0,13.5f,0),controlBlockSmooth), ptLfAtten=ptLowAtten_.next(finiteClamp(get(kPtLowAtten),0,17.5f,0),controlBlockSmooth);
        const float ptHfF=smoothLog(ptHighFreq_.v,ptHighFrequency(getInt(kPtHighFreq)),controlBlockSmooth); ptHighFreq_.snap(ptHfF);
        const float ptHfBoost=ptHighBoost_.next(finiteClamp(get(kPtHighBoost),0,18,0),controlBlockSmooth), ptBandwidth=ptBandwidth_.next(finiteClamp(get(kPtBandwidth),0,100,50),controlBlockSmooth);
        const float ptHfAttF=smoothLog(ptHighAttenFreq_.v,ptHighAttenFrequency(getInt(kPtHighAttenFreq)),controlBlockSmooth); ptHighAttenFreq_.snap(ptHfAttF);
        const float ptHfAtten=ptHighAtten_.next(finiteClamp(get(kPtHighAtten),0,16,0),controlBlockSmooth);
        const float ptLowBoostAlpha=1.0f-std::exp(-2.0f*kPi*ptLfF/sr_);
        const float ptLowAttenAlpha=1.0f-std::exp(-2.0f*kPi*ptLfF*1.41421356f/sr_);
        const float ptHighAttenAlpha=1.0f-std::exp(-2.0f*kPi*ptHfAttF/sr_);
        const float ptLowBoostGain=db(ptLfBoost), ptLowAttenGain=db(-ptLfAtten), ptHighAttenGain=db(-ptHfAtten);
        const float ptInteractionDb=(ptLfBoost>0 && ptLfAtten>0)?5.0f*(1.0f-std::exp(-std::fmin(ptLfBoost,ptLfAtten)/5.0f)):0.0f;
        const float ptHfQ=1.60f-1.15f*(ptBandwidth*0.01f);
        for(int c=0;c<2;++c) {
            ch_[c].ptHighBoost.resonantBoost(sr_,ptHfF,ptHfQ,ptHfBoost*(0.5f+0.5f*(1.0f-ptBandwidth*0.01f)));
            ch_[c].ptLfInteraction.bell(sr_,finiteClamp(ptLfF*8.0f,100.0f,sr_*0.45f,800.0f),0.70f,-ptInteractionDb);
            ch_[c].lf.shelf(sr_,lfF,lfG,false);
            ch_[c].lm.bell(sr_,lmF,lmQv,lmG);
            ch_[c].hm.bell(sr_,hmF,hmQv,hmG);
            ch_[c].hf.shelf(sr_,hfF,hfG,true);
        }
        // The shared HP/LP section uses the same 10 ms logarithmic glide and
        // 64-frame coefficient cadence in both Stratt plugins.
        for(int blockStart=0;blockStart<n;blockStart+=64) {
            const int blockFrames=mpc::clamp(n-blockStart,1,64);
            const float filterSmooth=1.0f-std::exp(-static_cast<float>(blockFrames)/(0.010f*std::fmax(sr_,1.0f)));
            const float hp=smoothLog(hpFreq_.v,hpTarget,filterSmooth); hpFreq_.snap(hp);
            const float lp=smoothLog(lpFreq_.v,lpTarget,filterSmooth); lpFreq_.snap(lp);
            for(int c=0;c<2;++c) { ch_[c].hp.setCutoff(sr_,hp); ch_[c].lp.setCutoff(sr_,lp); }
            const int blockEnd=blockStart+blockFrames;
            for(int i=blockStart;i<blockEnd;++i) {
            const float ig=input_.next(inGain), og=output_.next(outGain);
            const float pg=preamp_.next(preampGain), fg=fader_.next(faderGain);
            const float mix=circuitMix_.next(cmix);
            const float consoleFader=consoleFader_.next(consoleTarget,consoleSmoothing);
            const float l=(ctx.inputs&&ctx.numInputs>0&&ctx.inputs[0])?ctx.inputs[0][i]:0.0f;
            const float r=(ctx.inputs&&ctx.numInputs>1&&ctx.inputs[1])?ctx.inputs[1][i]:0.0f;
            const float consoleL=ch_[0].console.process(finiteClamp(l,-16,16,0),consoleFader);
            const float consoleR=ch_[1].console.process(finiteClamp(r,-16,16,0),consoleFader);
            const float a=consoleL*ig*pg*phaseL;
            const float b=consoleR*ig*pg*phaseR;
            float xL=(a*ilL+b*irL), xR=(a*ilR+b*irR);
            float *samples[2]={&xL,&xR};
            // Clean filtering and EQ precede the linked dynamics detector.
            for(int ch=0;ch<2;++ch) {
                Channel &s=ch_[ch]; float x=*samples[ch];
                x=s.hp.process(x); x=s.lp.process(x);
                s.ptLowBoostState+=ptLowBoostAlpha*(x-s.ptLowBoostState); x+=(ptLowBoostGain-1.0f)*s.ptLowBoostState;
                s.ptLowAttenState+=ptLowAttenAlpha*(x-s.ptLowAttenState); x+=(ptLowAttenGain-1.0f)*s.ptLowAttenState;
                x=s.ptLfInteraction.run(x); x=s.ptHighBoost.run(x);
                s.ptHighAttenState+=ptHighAttenAlpha*(x-s.ptHighAttenState); x=s.ptHighAttenState+ptHighAttenGain*(x-s.ptHighAttenState);
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
            const float targetComp=compCurve_[curveIndex]+(compCurve_[curveIndex<256?curveIndex+1:256]-compCurve_[curveIndex])*curveFrac;
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
                Channel &s=ch_[ch];
                *samples[ch]=*samples[ch]*gateGain_*compGain_*makeup;
                const float clean=*samples[ch];
                // User-controlled drive colour remains a separate parallel stage.
                // Its Mix=0 setting is exactly the clean dynamics signal.
                const float driveWet=circuit(clean,drive);
                *samples[ch]=clean+(driveWet-clean)*mix;
            }
            // Permanent, invisible valve-amplifier coloration: independent of
            // Circuit Mix, after all tone/dynamics stages and before output gain.
            // The same ADAA triode-inspired stage is used by StrattPultek.
            for(int ch=0;ch<2;++ch)
                *samples[ch]=ch_[ch].tubeStage(*samples[ch],valveDcCoef_);
            xL*=og*fg; xR*=og*fg;
            if(ctx.numOutputs==1) ctx.outputs[0][i]=0.5f*(xL+xR);
            else {
                if(ctx.outputs[0]) ctx.outputs[0][i]=xL*oL;
                if(ctx.outputs[1]) ctx.outputs[1][i]=xR*oR;
                for(int c=2;c<ctx.numOutputs;++c) if(ctx.outputs[c]) ctx.outputs[c][i]=0.0f;
            }
            }
        }
    }
private:
    static mpc::PluginInfo makeInfo() {
        mpc::PluginInfo x; x.name="StrattTrack"; x.vendor="Stratt";
        x.uniqueId=MPC_FOURCC('S','4','X','3'); x.version=1140;
        x.isInstrument=false; x.numInputs=2; x.numOutputs=2; x.tailSamples=1; return x;
    }
    Channel ch_[2]; float sr_=44100;
    Smooth output_,input_,preamp_,fader_,circuitMix_,consoleFader_;
    Smooth gateThreshDb_,gateRangeDb_,compThreshDb_,compRatio_,compMakeupDb_,drive_;
    Smooth panL_,panR_,outPan_,phaseL_,phaseR_;
    Smooth hpFreq_,lpFreq_,lfFreq_,lfGain_,lmFreq_,lmGain_,lmQ_,hmFreq_,hmGain_,hmQ_,hfFreq_,hfGain_;
    Smooth ptLowFreq_,ptLowBoost_,ptLowAtten_,ptHighFreq_,ptHighBoost_,ptBandwidth_,ptHighAttenFreq_,ptHighAtten_;
    // Linked dynamics state is separate from per-channel filter/colour state.
    float detector_=0.0f, gateDetector_=0.0f, gateGain_=1.0f, compGain_=1.0f;
    bool gateOpen_=true;
    float compCurve_[257]{};
    float compRatioCached_=-1.0f;
    float valveDcCoef_=0.999572f;
};
MPC_PLUGIN_ENTRY(StrattTrack)
