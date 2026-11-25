// StrattPultek — Pultec-inspired passive EQ curves + low-cost harmonic colour.
// SPDX-License-Identifier: MIT
//
// Signal path per channel:
//   input -> LF shelf boost -> LF resonant attenuation ("Pultec trick")
//         -> HF bell boost -> HF shelf attenuation -> tube-like soft saturation
//         -> output trim / dry-wet.
// The EQ curves are practical digital approximations of the EQP-1A control
// topology, not a component-by-component SPICE or transformer model. Frequency
// choices and interaction are exposed; calibration against a measured hardware
// unit remains a future validation step.

#include "mpc_plugin.h"
#include "dsp/biquad.h"
#include "dsp/smoother.h"

#include <cmath>

using namespace mpc::dsp;

namespace {
enum Params
{
    kOutput = 0, kLowFreq, kLowBoost, kLowAtten,
    kHighFreq, kHighBoost, kBandwidth,
    kHighAttenFreq, kHighAtten,
    kGrain, kDrive, kMix, kNumParams
};

static float dbToGain(float db)
{
    return std::pow(10.0f, db * 0.05f);
}

// C1-continuous polynomial soft clip. It is linear around zero, transitions
// smoothly from unity slope at |x|=1 to zero slope at |x|=3, then remains
// bounded at 1.5. Its closed-form antiderivative makes first-order ADAA cheap:
// no transcendental functions, allocations, or extra oversampling buffers.
static inline float softClip(float x)
{
    const float a = std::fabs(x);
    if (a <= 1.0f) return x;
    if (a >= 3.0f) return std::copysign(1.5f, x);
    const float t = 0.5f * (a - 1.0f);
    const float y = 1.0f + 2.0f * t - 2.5f * t * t + t * t * t;
    return std::copysign(y, x);
}

// Even antiderivative of softClip(). The ADAA divided difference averages
// the waveshaper over the previous/current input interval, suppressing
// harmonics above Nyquist before they fold back as audible aliasing.
static inline float softClipIntegral(float x)
{
    const float a = std::fabs(x);
    if (a <= 1.0f) return 0.5f * a * a;
    if (a >= 3.0f) return (10.0f / 3.0f) + 1.5f * (a - 3.0f);
    const float t = 0.5f * (a - 1.0f);
    const float t2 = t * t;
    return 0.5f + 2.0f * t + 2.0f * t2 - (5.0f / 3.0f) * t * t2 + 0.5f * t2 * t2;
}

static inline float adaaSoftClip(float x, float previousX)
{
    const float delta = x - previousX;
    if (std::fabs(delta) < 1.0e-4f)
        return softClip(0.5f * (x + previousX));
    return (softClipIntegral(x) - softClipIntegral(previousX)) / delta;
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

struct Channel
{
    Biquad lowBoost, lowDip, highBoost, highAtten;
    float previousDriveInput = 0.0f;
    void reset()
    {
        lowBoost.reset(); lowDip.reset();
        highBoost.reset(); highAtten.reset();
        previousDriveInput = 0.0f;
    }
};
} // namespace

class StrattPultek : public mpc::Plugin
{
public:
    StrattPultek() : mpc::Plugin(makeInfo())
    {
        using namespace mpc;
        // Index 0 is deliberately a harmless master output trim: the MPC
        // toggles parameter 0 during prepare/load. Keep this index permanent.
        addParam(floatParam("output", "Output", -12.0f, 12.0f, 0.0f, "dB", 1));
        addParam(choiceParam("low_freq", "Low Freq", {"20", "30", "60", "100"}, 2));
        addParam(floatParam("low_boost", "Low Boost", 0.0f, 10.0f, 0.0f, "dB", 1));
        addParam(floatParam("low_atten", "Low Atten", 0.0f, 10.0f, 0.0f, "dB", 1));
        addParam(choiceParam("high_freq", "High Freq", {"3k", "4k", "5k", "8k", "10k", "12k", "16k"}, 3));
        addParam(floatParam("high_boost", "High Boost", 0.0f, 10.0f, 0.0f, "dB", 1));
        addParam(floatParam("bandwidth", "Bandwidth", 0.0f, 100.0f, 50.0f, "%", 0));
        addParam(choiceParam("hf_att_freq", "HF Att Freq", {"5k", "10k", "20k"}, 1));
        addParam(floatParam("hf_atten", "HF Atten", 0.0f, 10.0f, 0.0f, "dB", 1));
        addParam(floatParam("grain", "Grain", 0.0f, 100.0f, 25.0f, "%", 0));
        addParam(floatParam("drive", "Drive", 0.0f, 100.0f, 20.0f, "%", 0));
        addParam(floatParam("mix", "Mix", 0.0f, 100.0f, 100.0f, "%", 0));

        addProgram("Classic", {0.0f, 2.0f, 3.0f, 2.0f, 3.0f, 3.0f, 50.0f, 1.0f, 2.0f, 20.0f, 15.0f, 100.0f});
        addProgram("Low Lift", {0.0f, 2.0f, 5.0f, 3.0f, 3.0f, 0.0f, 55.0f, 1.0f, 0.0f, 12.0f, 10.0f, 100.0f});
        addProgram("Air", {0.0f, 1.0f, 0.0f, 0.0f, 5.0f, 5.0f, 65.0f, 2.0f, 2.0f, 18.0f, 12.0f, 100.0f});
        addProgram("Warm Drive", {0.0f, 2.0f, 2.0f, 1.0f, 2.0f, 2.0f, 40.0f, 1.0f, 1.0f, 65.0f, 55.0f, 100.0f});
        addProgram("Subtle Glue", {0.0f, 2.0f, 1.5f, 1.0f, 3.0f, 1.5f, 55.0f, 1.0f, 1.0f, 30.0f, 18.0f, 80.0f});
    }

    void prepare(double sampleRate, int /*maxBlockSize*/) override
    {
        sampleRate_ = (std::isfinite(sampleRate) && sampleRate >= 8000.0 && sampleRate <= 384000.0)
                        ? sampleRate : 44100.0;
        output_.setTime(15.0f, sampleRate_);
        grain_.setTime(15.0f, sampleRate_);
        drive_.setTime(15.0f, sampleRate_);
        mix_.setTime(15.0f, sampleRate_);
        reset();
    }

    void reset() override
    {
        for (Channel& c : channels_) c.reset();
        output_.snap(dbToGain(get(kOutput)));
        grain_.snap(get(kGrain) * 0.01f);
        drive_.snap(get(kDrive) * 0.01f);
        mix_.snap(get(kMix) * 0.01f);

        // State restoration and program changes must take effect immediately.
        eqLowFreq_ = lowFrequency(getInt(kLowFreq));
        eqLowBoost_ = get(kLowBoost);
        eqLowAtten_ = get(kLowAtten);
        eqHighFreq_ = highFrequency(getInt(kHighFreq));
        eqHighBoost_ = get(kHighBoost);
        eqBandwidth_ = get(kBandwidth);
        eqHighAttenFreq_ = attenuationFrequency(getInt(kHighAttenFreq));
        eqHighAtten_ = get(kHighAtten);
        updateEq();
    }

    void process(mpc::ProcessContext& ctx) override
    {
        if (!ctx.outputs || ctx.numFrames <= 0) return;

        smoothEq(ctx.numFrames);
        updateEq();
        output_.setTarget(dbToGain(get(kOutput)));
        grain_.setTarget(get(kGrain) * 0.01f);
        drive_.setTarget(get(kDrive) * 0.01f);
        mix_.setTarget(get(kMix) * 0.01f);

        const int channels = mpc::clamp(ctx.numOutputs, 0, 2);
        for (int i = 0; i < ctx.numFrames; ++i)
        {
            const float outGain = output_.process();
            const float colour = grain_.process();
            const float drive = drive_.process();
            const float mix = mix_.process();
            const float driveGain = 1.0f + 2.5f * drive;

            for (int ch = 0; ch < channels; ++ch)
            {
                if (!ctx.outputs[ch]) continue;
                const float dry = (ctx.inputs && ch < ctx.numInputs && ctx.inputs[ch])
                                    ? ctx.inputs[ch][i] : 0.0f;
                float wet = channels_[ch].lowBoost.process(dry);
                wet = channels_[ch].lowDip.process(wet);
                wet = channels_[ch].highBoost.process(wet);
                wet = channels_[ch].highAtten.process(wet);

                // Drive=0 is exactly clean. Grain controls the colour blend;
                // both controls therefore reach a genuinely neutral setting.
                const float driveInput = wet * driveGain;
                const float saturated = adaaSoftClip(driveInput, channels_[ch].previousDriveInput) / driveGain;
                channels_[ch].previousDriveInput = driveInput;
                wet += (saturated - wet) * (colour * drive);
                const float processed = dry + (wet - dry) * mix;
                ctx.outputs[ch][i] = processed * outGain;
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
        i.version = 1000;
        i.isInstrument = false;
        i.numInputs = 2;
        i.numOutputs = 2;
        i.tailSamples = 1;
        return i;
    }

    void smoothEq(int frames)
    {
        eqLowFreq_ = smoothFrequency(eqLowFreq_, lowFrequency(getInt(kLowFreq)), frames, sampleRate_);
        eqLowBoost_ = smoothBlock(eqLowBoost_, get(kLowBoost), frames, sampleRate_);
        eqLowAtten_ = smoothBlock(eqLowAtten_, get(kLowAtten), frames, sampleRate_);
        eqHighFreq_ = smoothFrequency(eqHighFreq_, highFrequency(getInt(kHighFreq)), frames, sampleRate_);
        eqHighBoost_ = smoothBlock(eqHighBoost_, get(kHighBoost), frames, sampleRate_);
        eqBandwidth_ = smoothBlock(eqBandwidth_, get(kBandwidth), frames, sampleRate_);
        eqHighAttenFreq_ = smoothFrequency(eqHighAttenFreq_, attenuationFrequency(getInt(kHighAttenFreq)), frames, sampleRate_);
        eqHighAtten_ = smoothBlock(eqHighAtten_, get(kHighAtten), frames, sampleRate_);
    }

    void updateEq()
    {
        // Broad -> lower Q; Narrow -> higher Q.
        const float q = 1.60f - 1.15f * (eqBandwidth_ * 0.01f);
        const BiquadCoeffs lb = BiquadCoeffs::lowShelf(sampleRate_, eqLowFreq_, 0.7071f, eqLowBoost_);
        // The Pultec low boost/attenuation interaction is approximated by a
        // broad dip above the selected LF shelf, rather than a second shelf.
        const BiquadCoeffs ld = BiquadCoeffs::peak(sampleRate_, mpc::clamp(eqLowFreq_ * 1.55f, 25.0f, 250.0f),
                                                   0.78f, -eqLowAtten_);
        const BiquadCoeffs hb = BiquadCoeffs::peak(sampleRate_, eqHighFreq_, q, eqHighBoost_);
        const BiquadCoeffs ha = BiquadCoeffs::highShelf(sampleRate_, eqHighAttenFreq_, 0.7071f, -eqHighAtten_);
        for (Channel& c : channels_)
        {
            c.lowBoost.setCoeffs(lb); c.lowDip.setCoeffs(ld);
            c.highBoost.setCoeffs(hb); c.highAtten.setCoeffs(ha);
        }
    }

    double sampleRate_ = 44100.0;
    float eqLowFreq_ = 60.0f, eqLowBoost_ = 0.0f, eqLowAtten_ = 0.0f;
    float eqHighFreq_ = 8000.0f, eqHighBoost_ = 0.0f, eqBandwidth_ = 50.0f;
    float eqHighAttenFreq_ = 10000.0f, eqHighAtten_ = 0.0f;
    Channel channels_[2];
    Smoother output_, grain_, drive_, mix_;
};

MPC_PLUGIN_ENTRY(StrattPultek)
