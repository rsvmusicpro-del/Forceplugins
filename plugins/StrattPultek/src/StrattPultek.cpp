// StrattPultek — Pultec-inspired passive EQ curves + calibrated Cleansweep filtering.
// SPDX-License-Identifier: MIT
//
// Signal path per channel:
//   input -> 6 dB/oct HP -> 6 dB/oct LP -> LF boost/attenuation shelves
//         -> HF bell boost -> HF shelf attenuation -> asymmetric harmonic saturation
//         -> global EQ dry/wet -> fixed valve colour -> Output master gain -> final Fader.
// The EQ curves are practical digital approximations of the EQP-1A control
// topology. Both LF controls are shelving filters; the attenuation shelf has
// a higher corner frequency, so the cascaded response creates the familiar
// low-end lift plus upper-bass dip. This models the topology, not every passive
// component or transformer. Calibration against a measured hardware unit
// remains a separate validation step.

#include "mpc_plugin.h"
#include "dsp/biquad.h"
#include "dsp/smoother.h"
#include "dsp/onepole.h"
#include "dsp/cleansweep_filter.h"

#include <cmath>

using namespace mpc::dsp;

namespace {
enum Params
{
    kOutput = 0, kLowFreq, kLowBoost, kLowAtten,
    kHighFreq, kHighBoost, kBandwidth,
    kHighAttenFreq, kHighAtten,
    kGrain, kDrive, kMix, kDriveMix, kFader,
    kHpFreq, kLpFreq, kNumParams
};

static float dbToGain(float db)
{
    return std::pow(10.0f, db * 0.05f);
}

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

// Fundamental-preserving makeup for the Drive waveshaper.
// Calibrate against a 0.5-peak sine (-6.02 dBFS): integrate the odd part
// of the static transfer over one half-cycle, then restore its fundamental
// amplitude after the existing 1/driveGain normalization. This is a numerical
// level-matching reference, not a claim of matching a measured analogue unit.
static float calculateDriveMakeupGain(float driveAmount)
{
    constexpr int intervals = 256;
    constexpr double pi = 3.1415926535897932384626433832795;
    constexpr double referencePeak = 0.5;
    const double driveGain = 1.0 + 2.5 * static_cast<double>(mpc::clamp(driveAmount, 0.0f, 1.0f));
    const double peak = referencePeak * driveGain;
    double sum = 0.0;
    for (int i = 0; i <= intervals; ++i)
    {
        const double phase = pi * static_cast<double>(i) / intervals;
        const double sine = std::sin(phase);
        const double value = softClipTransfer(peak * sine, 0.0) * sine;
        const int weight = (i == 0 || i == intervals) ? 1 : ((i & 1) ? 4 : 2);
        sum += static_cast<double>(weight) * value;
    }
    const double integral = sum * pi / (3.0 * intervals);
    const double fundamental = (2.0 / pi) * integral;
    const double compensation = referencePeak * driveGain / std::fmax(fundamental, 1.0e-12);
    return static_cast<float>(mpc::clamp(static_cast<float>(compensation), 1.0f, 1.12f));
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

// Smooth a control once per block while preserving the same time constant for
// variable host block sizes (the MPC may call process() with 32..512 frames).
static float smoothBlock(float current, float target, int frames, double sampleRate)
{
    if (frames <= 0 || sampleRate <= 0.0) return target;
    const float timeSamples = 0.015f * static_cast<float>(sampleRate);
    const float alpha = 1.0f - std::exp(-static_cast<float>(frames) / timeSamples);
    return current + alpha * (target - current);
}

// Frequency controls are smoothed in log space so octave-sized movements
// have a consistent perceptual glide instead of spending most of the glide
// near one end of a large linear-Hz interval.
static float smoothFrequency(float current, float target, int frames, double sampleRate)
{
    if (frames <= 0 || sampleRate <= 0.0) return target;
    const float timeSamples = 0.015f * static_cast<float>(sampleRate);
    const float alpha = 1.0f - std::exp(-static_cast<float>(frames) / timeSamples);
    const float safeCurrent = mpc::clamp(current, 1.0f, 192000.0f);
    const float safeTarget = mpc::clamp(target, 1.0f, 192000.0f);
    return std::exp(std::log(safeCurrent) + alpha * (std::log(safeTarget) - std::log(safeCurrent)));
}

static float lowFrequency(int index)
{
    static const float f[] = {20.0f, 30.0f, 60.0f, 100.0f};
    return f[mpc::clamp(index, 0, 3)];
}
static float highFrequency(int index)
{
    static const float f[] = {3000.0f, 4000.0f, 5000.0f, 8000.0f,
                              10000.0f, 12000.0f, 16000.0f};
    return f[mpc::clamp(index, 0, 6)];
}
static float attenuationFrequency(int index)
{
    static const float f[] = {5000.0f, 10000.0f, 20000.0f};
    return f[mpc::clamp(index, 0, 2)];
}

// Find the one-pole shelf pole that places the selected frequency near the
// hardware's documented mid-attenuation point (~-12 dB for a -21 dB maximum).
// Solve the actual discrete-time OnePoleLP response, so the 20 kHz selector
// remains calibrated near Nyquist at the MPC's 44.1 kHz sample rate.
static float highAttenuationPole(float selectorHz, double sampleRate)
{
    const float sr = static_cast<float>(sampleRate);
    const float frequency = mpc::clamp(selectorHz, 1.0f, 0.49f * sr);
    const float omega = 6.2831853071795864769f * frequency / sr;
    const float cw = std::cos(omega);
    const float sw = std::sin(omega);
    const float shelfGain = dbToGain(-20.0f);
    const float target = dbToGain(-20.0f * (12.0f / 21.0f));
    float low = 1.0f;
    float high = 0.49f * sr;

    // Monotonic in pole frequency; 24 iterations exceed float precision here.
    for (int i = 0; i < 24; ++i)
    {
        const float pole = 0.5f * (low + high);
        const float a = 1.0f - std::exp(-6.2831853071795864769f * pole / sr);
        const float oneMinusA = 1.0f - a;
        const float denReal = 1.0f - oneMinusA * cw;
        const float denImag = oneMinusA * sw;
        const float den2 = denReal * denReal + denImag * denImag;
        const float lpReal = a * denReal / den2;
        const float lpImag = a * denImag / den2;
        const float hReal = shelfGain + (1.0f - shelfGain) * lpReal;
        const float hImag = (1.0f - shelfGain) * lpImag;
        const float magnitude = std::sqrt(hReal * hReal + hImag * hImag);
        if (magnitude < target) low = pole;
        else high = pole;
    }
    return 0.5f * (low + high);
}

struct Channel
{
    CleansweepOnePole highpass{CleansweepOnePole::Mode::Highpass};
    CleansweepOnePole lowpass{CleansweepOnePole::Mode::Lowpass};
    OnePoleLP lowBoost, lowAtten, highAtten;
    Biquad highBoost;
    float previousDriveInput = 0.0f;
    bool hasPreviousDriveInput = false;
    float previousDcInput = 0.0f, previousDcOutput = 0.0f;
    // Independent, permanently enabled valve-stage memory. It sits after the
    // complete EQ/colour/mix/output-trim path and immediately before the fader.
    float previousTubeInput = 0.0f;
    bool hasPreviousTubeInput = false;
    float previousTubeDcInput = 0.0f, previousTubeDcOutput = 0.0f;
    void reset()
    {
        highpass.reset(); lowpass.reset();
        lowBoost.reset(); lowAtten.reset();
        highBoost.reset(); highAtten.reset();
        previousDriveInput = previousDcInput = previousDcOutput = 0.0f;
        hasPreviousDriveInput = false;
        previousTubeInput = previousTubeDcInput = previousTubeDcOutput = 0.0f;
        hasPreviousTubeInput = false;
    }
    float removeDc(float x, float pole)
    {
        const float y = x - previousDcInput + pole * previousDcOutput;
        previousDcInput = x;
        previousDcOutput = y;
        return y;
    }
    float tubeStage(float x, float pole)
    {
        // Fixed, non-user-adjustable triode-inspired transfer. The input
        // gain moves louder peaks gradually into the C2 soft knee; the asymmetric
        // term creates predominantly even-order colour. ADAA's closed-form
        // primitive limits aliasing without oversampling or per-sample maths
        // beyond the polynomial. Divide by drive to preserve nominal gain.
        constexpr float tubeDrive = 1.15f;
        constexpr float tubeAsymmetry = 0.045f;
        const float driven = x * tubeDrive;
        const float previous = hasPreviousTubeInput ? previousTubeInput : driven;
        const float shaped = adaaSoftClip(driven, previous, tubeAsymmetry) / tubeDrive;
        previousTubeInput = driven;
        hasPreviousTubeInput = true;
        // Remove the small DC bias generated by the asymmetric transfer.
        const float y = shaped - previousTubeDcInput + pole * previousTubeDcOutput;
        previousTubeDcInput = shaped;
        previousTubeDcOutput = y;
        return y;
    }
};
} // namespace

class StrattPultek : public mpc::Plugin
{
public:
    StrattPultek() : mpc::Plugin(makeInfo())
    {
        using namespace mpc;
        // Index 0 is deliberately a tone-neutral master output gain: the MPC
        // toggles parameter 0 during prepare/load. Keep this index permanent.
        addParam(floatParam("output", "Output", -12.0f, 12.0f, 0.0f, "dB", 1));
        addParam(choiceParam("low_freq", "Low Freq", {"20", "30", "60", "100"}, 2));
        addParam(floatParam("low_boost", "Low Boost", 0.0f, 14.0f, 0.0f, "dB", 1));
        addParam(floatParam("low_atten", "Low Atten", 0.0f, 18.0f, 0.0f, "dB", 1));
        addParam(choiceParam("high_freq", "High Freq", {"3k", "4k", "5k", "8k", "10k", "12k", "16k"}, 3));
        addParam(floatParam("high_boost", "High Boost", 0.0f, 20.0f, 0.0f, "dB", 1));
        addParam(floatParam("bandwidth", "Bandwidth", 0.0f, 100.0f, 50.0f, "%", 0));
        addParam(choiceParam("hf_att_freq", "HF Att Freq", {"5k", "10k", "20k"}, 1));
        addParam(floatParam("hf_atten", "HF Atten", 0.0f, 20.0f, 0.0f, "dB", 1));
        addParam(floatParam("grain", "Grain", 0.0f, 100.0f, 25.0f, "%", 0));
        addParam(floatParam("drive", "Drive", 0.0f, 100.0f, 20.0f, "%", 0));
        addParam(floatParam("mix", "Mix", 0.0f, 100.0f, 100.0f, "%", 0));
        // Appended to preserve all existing parameter indices and saved projects.
        addParam(floatParam("drive_mix", "Drive Mix", 0.0f, 100.0f, 100.0f, "%", 0));
        // Dedicated post-chain master fader; appended to preserve saved projects.
        addParam(floatParam("fader", "Fader", -60.0f, 6.0f, 0.0f, "dB", 1));
        // Appended only: preserve the established 0..13 parameter contract.
        addParam(logParam("hp_freq", "HP Freq", 20.0f, 11000.0f, 20.0f, "Hz", 0));
        addParam(logParam("lp_freq", "LP Freq", 20.0f, 22000.0f, 22000.0f, "Hz", 0));

        addProgram("Classic", {0.0f, 2.0f, 3.0f, 2.0f, 3.0f, 3.0f, 50.0f, 1.0f, 2.0f, 20.0f, 15.0f, 100.0f, 100.0f, 0.0f, 20.0f, 22000.0f});
        addProgram("Low Lift", {0.0f, 2.0f, 5.0f, 3.0f, 3.0f, 0.0f, 55.0f, 1.0f, 0.0f, 12.0f, 10.0f, 100.0f, 100.0f, 0.0f, 20.0f, 22000.0f});
        addProgram("Air", {0.0f, 1.0f, 0.0f, 0.0f, 5.0f, 5.0f, 65.0f, 2.0f, 2.0f, 18.0f, 12.0f, 100.0f, 100.0f, 0.0f, 20.0f, 22000.0f});
        addProgram("Warm Drive", {0.0f, 2.0f, 2.0f, 1.0f, 2.0f, 2.0f, 40.0f, 1.0f, 1.0f, 65.0f, 55.0f, 100.0f, 100.0f, 0.0f, 20.0f, 22000.0f});
        addProgram("Subtle Glue", {0.0f, 2.0f, 1.5f, 1.0f, 3.0f, 1.5f, 55.0f, 1.0f, 1.0f, 30.0f, 18.0f, 80.0f, 100.0f, 0.0f, 20.0f, 22000.0f});
    }

    void prepare(double sampleRate, int /*maxBlockSize*/) override
    {
        sampleRate_ = (std::isfinite(sampleRate) && sampleRate >= 8000.0 && sampleRate <= 384000.0)
                        ? sampleRate : 44100.0;
        dcPole_ = std::exp(-6.2831853071795864769f * 3.0f / static_cast<float>(sampleRate_));
        // Precompute the Drive's -6 dBFS sine fundamental makeup curve outside
        // the audio callback. Per-sample use is a bounded table interpolation.
        for (int i = 0; i <= kDriveMakeupSteps; ++i)
            driveMakeupTable_[i] = calculateDriveMakeupGain(static_cast<float>(i) / kDriveMakeupSteps);
        // The three selector poles are solved once per prepare, never in process().
        for (int i = 0; i < 3; ++i)
            highAttenPoleChoices_[i] = highAttenuationPole(attenuationFrequency(i), sampleRate_);
        output_.setTime(15.0f, sampleRate_);
        grain_.setTime(15.0f, sampleRate_);
        drive_.setTime(15.0f, sampleRate_);
        mix_.setTime(15.0f, sampleRate_);
        driveMix_.setTime(15.0f, sampleRate_);
        fader_.setTime(15.0f, sampleRate_);
        reset();
    }

    void reset() override
    {
        for (Channel& c : channels_) c.reset();
        output_.snap(dbToGain(readParam(kOutput, 0.0f, -12.0f, 12.0f)));
        grain_.snap(readParam(kGrain, 25.0f, 0.0f, 100.0f) * 0.01f);
        drive_.snap(readParam(kDrive, 20.0f, 0.0f, 100.0f) * 0.01f);
        mix_.snap(readParam(kMix, 100.0f, 0.0f, 100.0f) * 0.01f);
        driveMix_.snap(readParam(kDriveMix, 100.0f, 0.0f, 100.0f) * 0.01f);
        fader_.snap(dbToGain(readParam(kFader, 0.0f, -60.0f, 6.0f)));

        // State restoration and program changes must take effect immediately.
        eqLowFreq_ = lowFrequency(readChoice(kLowFreq, 2, 3));
        eqLowBoost_ = readParam(kLowBoost, 0.0f, 0.0f, 14.0f);
        eqLowAtten_ = readParam(kLowAtten, 0.0f, 0.0f, 18.0f);
        eqHighFreq_ = highFrequency(readChoice(kHighFreq, 3, 6));
        eqHighBoost_ = readParam(kHighBoost, 0.0f, 0.0f, 20.0f);
        eqBandwidth_ = readParam(kBandwidth, 50.0f, 0.0f, 100.0f);
        eqHighAttenPole_ = highAttenPoleChoices_[readChoice(kHighAttenFreq, 1, 2)];
        eqHighAtten_ = readParam(kHighAtten, 0.0f, 0.0f, 20.0f);
        hpFreq_ = readParam(kHpFreq, 20.0f, 20.0f, 11000.0f);
        lpFreq_ = readParam(kLpFreq, 22000.0f, 20.0f, 22000.0f);
        updateEq();
    }

    void process(mpc::ProcessContext& ctx) override
    {
        if (!ctx.outputs || ctx.numFrames <= 0) return;

        // Parameter targets are read once per host block. Filter coefficients
        // are refreshed in bounded 64-frame segments, reducing block-edge
        // zippering even when the MPC uses 256/512-frame audio blocks.
        output_.setTarget(dbToGain(readParam(kOutput, 0.0f, -12.0f, 12.0f)));
        grain_.setTarget(readParam(kGrain, 25.0f, 0.0f, 100.0f) * 0.01f);
        drive_.setTarget(readParam(kDrive, 20.0f, 0.0f, 100.0f) * 0.01f);
        mix_.setTarget(readParam(kMix, 100.0f, 0.0f, 100.0f) * 0.01f);
        driveMix_.setTarget(readParam(kDriveMix, 100.0f, 0.0f, 100.0f) * 0.01f);
        fader_.setTarget(dbToGain(readParam(kFader, 0.0f, -60.0f, 6.0f)));

        constexpr int kControlQuantum = 64;
        const int channels = mpc::clamp(ctx.numOutputs, 0, 2);
        for (int blockStart = 0; blockStart < ctx.numFrames; blockStart += kControlQuantum)
        {
            const int blockFrames = mpc::clamp(ctx.numFrames - blockStart, 1, kControlQuantum);
            smoothEq(blockFrames);
            updateEq();
            const int blockEnd = blockStart + blockFrames;
            for (int i = blockStart; i < blockEnd; ++i)
            {
            const float outGain = output_.process();
            const float colour = grain_.process();
            const float drive = drive_.process();
            const float mix = mix_.process();
            const float driveMix = driveMix_.process();
            const float finalFader = fader_.process();
            const float driveGain = 1.0f + 2.5f * drive;

            for (int ch = 0; ch < channels; ++ch)
            {
                if (!ctx.outputs[ch]) continue;
                const float dry = (ctx.inputs && ch < ctx.numInputs && ctx.inputs[ch])
                                    ? ctx.inputs[ch][i] : 0.0f;
                // Reject malformed host samples before they enter recursive DSP state.
                // Keep 24 dB of float headroom, but bound malformed finite host values
                // before they reach recursive filters and polynomial primitives.
                // Normal programme levels (including substantial inter-sample peaks)
                // remain untouched.
                constexpr float kMaxInputMagnitude = 16.0f;
                const float safeDry = std::isfinite(dry)
                    ? mpc::clamp(dry, -kMaxInputMagnitude, kMaxInputMagnitude) : 0.0f;
                // The original LF boost and attenuation, plus HF attenuation,
                // are gentle first-order shelves (6 dB/oct), not RBJ 2-pole shelves.
                // bx_cleansweep V2-style first-order 6 dB/octave filters, HP then LP.
                // The global EQ Mix remains the final parallel blend against dry.
                float filtered = channels_[ch].highpass.process(safeDry);
                filtered = channels_[ch].lowpass.process(filtered);
                float wet = filtered + (lowBoostGain_ - 1.0f) * channels_[ch].lowBoost.process(filtered);
                wet += (lowAttenGain_ - 1.0f) * channels_[ch].lowAtten.process(wet);
                wet = channels_[ch].highBoost.process(wet);
                const float hfLow = channels_[ch].highAtten.process(wet);
                wet = hfLow + highAttenGain_ * (wet - hfLow);

                // Drive=0 is exactly clean. Grain controls the colour blend;
                // both controls therefore reach a genuinely neutral setting.
                const float driveInput = wet * driveGain;
                // A small asymmetric term adds even-order harmonic content; a slow DC
                // blocker removes the resulting bias without disturbing the bass band.
                const float previousDriveInput = channels_[ch].hasPreviousDriveInput
                    ? channels_[ch].previousDriveInput : driveInput;
                const float saturated = channels_[ch].removeDc(
                    adaaSoftClip(driveInput, previousDriveInput, 0.08f),
                    dcPole_) / driveGain * driveMakeupGain(drive);
                channels_[ch].previousDriveInput = driveInput;
                channels_[ch].hasPreviousDriveInput = true;

                // Airwindows PurestDrive-inspired density weighting: the
                // previous/current sample sum naturally backs off at sharp
                // zero-crossings and lets sustained waveform body carry more
                // colour. Unlike the original sine shaper, this uses our
                // polynomial ADAA transfer and keeps the MPC CPU budget low.
                const float density = mpc::clamp(
                    std::fabs(previousDriveInput + driveInput) * 2.0f, 0.0f, 1.0f);
                const float driveWet = colour * drive * driveMix * density;
                wet += (saturated - wet) * driveWet;
                // Filter cutoffs remain independent of the Pultec/circuit wet-dry control.
                const float processed = filtered + (wet - filtered) * mix;
                // Permanent, invisible valve-amplifier coloration is deliberately
                // after every EQ/drive/mix/output-trim stage and directly before
                // the final master fader. It has no parameter or UI mapping.
                // Parameter 0 is the host-nudged master output. Apply it after
                // the nonlinear valve stage so automation/load nudges cannot alter tone.
                const float tubeOutput = channels_[ch].tubeStage(processed, dcPole_);
                // The dedicated Fader remains the final operation in the signal path.
                const float outputSample = tubeOutput * outGain * finalFader;
                if (std::isfinite(outputSample))
                {
                    ctx.outputs[ch][i] = outputSample;
                }
                else
                {
                    // Last-resort recovery: never leak NaN/Inf into the MPC mix
                    // bus, and clear only this channel's recursive history.
                    channels_[ch].reset();
                    ctx.outputs[ch][i] = 0.0f;
                }
            }
        }
        }
    }

private:
    static mpc::PluginInfo makeInfo()
    {
        mpc::PluginInfo i;
        i.name = "StrattPultek";
        i.vendor = "Stratt";
        i.uniqueId = MPC_FOURCC('S', 'o', '2', '1');
        i.version = 1907;
        i.isInstrument = false;
        i.numInputs = 2;
        i.numOutputs = 2;
        i.tailSamples = 1;
        return i;
    }

    // Host automation is normally finite, but a malformed VST host can send NaN/Inf.
    // Never let such values enter a smoother or turn a choice into an invalid index.
    float readParam(int index, float fallback, float minimum, float maximum) const
    {
        const float value = get(index);
        return std::isfinite(value) ? mpc::clamp(value, minimum, maximum) : fallback;
    }

    int readChoice(int index, int fallback, int maximum) const
    {
        const float value = readParam(index, static_cast<float>(fallback), 0.0f,
                                      static_cast<float>(maximum));
        return static_cast<int>(std::lround(value));
    }

    static constexpr int kDriveMakeupSteps = 100;

    float driveMakeupGain(float amount) const
    {
        const float position = mpc::clamp(amount, 0.0f, 1.0f) * kDriveMakeupSteps;
        const int index = mpc::clamp(static_cast<int>(position), 0, kDriveMakeupSteps - 1);
        const float fraction = position - static_cast<float>(index);
        return driveMakeupTable_[index]
             + (driveMakeupTable_[index + 1] - driveMakeupTable_[index]) * fraction;
    }

    void smoothEq(int frames)
    {
        hpFreq_ = smoothFrequency(hpFreq_, readParam(kHpFreq, 20.0f, 20.0f, 11000.0f), frames, sampleRate_);
        lpFreq_ = smoothFrequency(lpFreq_, readParam(kLpFreq, 22000.0f, 20.0f, 22000.0f), frames, sampleRate_);
        eqLowFreq_ = smoothFrequency(eqLowFreq_, lowFrequency(readChoice(kLowFreq, 2, 3)), frames, sampleRate_);
        eqLowBoost_ = smoothBlock(eqLowBoost_, readParam(kLowBoost, 0.0f, 0.0f, 14.0f), frames, sampleRate_);
        eqLowAtten_ = smoothBlock(eqLowAtten_, readParam(kLowAtten, 0.0f, 0.0f, 18.0f), frames, sampleRate_);
        eqHighFreq_ = smoothFrequency(eqHighFreq_, highFrequency(readChoice(kHighFreq, 3, 6)), frames, sampleRate_);
        eqHighBoost_ = smoothBlock(eqHighBoost_, readParam(kHighBoost, 0.0f, 0.0f, 20.0f), frames, sampleRate_);
        eqBandwidth_ = smoothBlock(eqBandwidth_, readParam(kBandwidth, 50.0f, 0.0f, 100.0f), frames, sampleRate_);
        eqHighAttenPole_ = smoothFrequency(eqHighAttenPole_, highAttenPoleChoices_[readChoice(kHighAttenFreq, 1, 2)], frames, sampleRate_);
        eqHighAtten_ = smoothBlock(eqHighAtten_, readParam(kHighAtten, 0.0f, 0.0f, 20.0f), frames, sampleRate_);
    }

    void updateEq()
    {
        // Broad -> lower Q; Narrow -> higher Q.
        const float q = 1.60f - 1.15f * (eqBandwidth_ * 0.01f);
        // Both original LF controls are shelving networks. Their corner
        // frequencies are deliberately offset: the attenuation shelf starts
        // about half an octave above the boost shelf. Cascading the two shelves
        // creates the characteristic low lift and upper-bass dip without an
        // invented peaking filter in the attenuation path.
        const float lowAttenCorner = mpc::clamp(eqLowFreq_ * 1.41421356f, 1.0f, 400.0f);
        // The selector is near the mid-attenuation point, not the -3 dB pole.
        // Solve the discrete-time first-order shelf at the selected frequency;
        // this matters especially for 20 kHz at the MPC's 44.1 kHz sample rate.
        const float hfAttenCorner = eqHighAttenPole_;
        // The hardware's HF boost maximum falls from about +20 dB (narrow) to
        // about +13.5 dB (broad). Approximate that bandwidth-dependent headroom.
        const float bandwidthGain = 1.0f - 0.325f * (eqBandwidth_ * 0.01f);
        const BiquadCoeffs hb = BiquadCoeffs::peak(sampleRate_, eqHighFreq_, q,
                                                   eqHighBoost_ * bandwidthGain);
        lowBoostGain_ = dbToGain(eqLowBoost_);
        lowAttenGain_ = dbToGain(-eqLowAtten_);
        highAttenGain_ = dbToGain(-eqHighAtten_);
        for (Channel& c : channels_)
        {
            c.highpass.setCutoff(hpFreq_, sampleRate_);
            c.lowpass.setCutoff(lpFreq_, sampleRate_);
            c.lowBoost.setCutoff(eqLowFreq_, sampleRate_);
            c.lowAtten.setCutoff(lowAttenCorner, sampleRate_);
            c.highBoost.setCoeffs(hb);
            c.highAtten.setCutoff(hfAttenCorner, sampleRate_);
        }
    }

    double sampleRate_ = 44100.0;
    float dcPole_ = 0.9995f; // 3 Hz DC blocker pole, recalculated in prepare()
    float eqLowFreq_ = 60.0f, eqLowBoost_ = 0.0f, eqLowAtten_ = 0.0f;
    float lowBoostGain_ = 1.0f, lowAttenGain_ = 1.0f, highAttenGain_ = 1.0f;
    float eqHighFreq_ = 8000.0f, eqHighBoost_ = 0.0f, eqBandwidth_ = 50.0f;
    float highAttenPoleChoices_[3] = { 1120.0f, 1911.0f, 2635.0f };
    float eqHighAttenPole_ = 1911.0f, eqHighAtten_ = 0.0f;
    float hpFreq_ = 20.0f, lpFreq_ = 22000.0f;
    Channel channels_[2];
    float driveMakeupTable_[kDriveMakeupSteps + 1] = {};
    Smoother output_, grain_, drive_, mix_, driveMix_, fader_;
};

MPC_PLUGIN_ENTRY(StrattPultek)
