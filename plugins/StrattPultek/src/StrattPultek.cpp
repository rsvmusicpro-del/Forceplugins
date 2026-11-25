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

// Fast rational tanh approximation. The bounded input avoids overflow and
// the function is called only in the colour stage, once per sample.
static inline float fastTanh(float x)
{
    x = mpc::clamp(x, -3.0f, 3.0f);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
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
    void reset()
    {
        lowBoost.reset(); lowDip.reset();
        highBoost.reset(); highAtten.reset();
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
        // toggles parameter 0 during prepare/load.
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
        sampleRate_ = sampleRate;
        output_.setTime(15.0f, sampleRate);
        grain_.setTime(15.0f, sampleRate);
        drive_.setTime(15.0f, sampleRate);
        mix_.setTime(15.0f, sampleRate);
        updateEq();
        reset();
    }

    void reset() override
    {
        for (Channel& c : channels_) c.reset();
        output_.snap(dbToGain(get(kOutput)));
        grain_.snap(get(kGrain) * 0.01f);
        drive_.snap(1.0f + get(kDrive) * 0.025f);
        mix_.snap(get(kMix) * 0.01f);
    }

    void process(mpc::ProcessContext& ctx) override
    {
        updateEq();
        output_.setTarget(dbToGain(get(kOutput)));
        grain_.setTarget(get(kGrain) * 0.01f);
        drive_.setTarget(1.0f + get(kDrive) * 0.025f);
        mix_.setTarget(get(kMix) * 0.01f);

        const int channels = mpc::clamp(ctx.numOutputs, 0, 2);
        for (int i = 0; i < ctx.numFrames; ++i)
        {
            const float outGain = output_.process();
            const float colour = grain_.process();
            const float drive = drive_.process();
            const float mix = mix_.process();

            for (int ch = 0; ch < channels; ++ch)
            {
                const float dry = (ctx.inputs && ch < ctx.numInputs && ctx.inputs[ch])
                                    ? ctx.inputs[ch][i] : 0.0f;
                float wet = channels_[ch].lowBoost.process(dry);
                wet = channels_[ch].lowDip.process(wet);
                wet = channels_[ch].highBoost.process(wet);
                wet = channels_[ch].highAtten.process(wet);

                // Unity small-signal gain; the rational tanh creates a gentle
                // odd-harmonic spectrum without oversampling or per-sample pow().
                const float saturated = fastTanh(wet * drive) / drive;
                wet += (saturated - wet) * colour;
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

    void updateEq()
    {
        const float lf = lowFrequency(getInt(kLowFreq));
        const float hf = highFrequency(getInt(kHighFreq));
        const float af = attenuationFrequency(getInt(kHighAttenFreq));
        const float lowBoost = get(kLowBoost);
        const float lowAtten = get(kLowAtten);
        const float highBoost = get(kHighBoost);
        const float highAtten = get(kHighAtten);
        // Broad -> lower Q; Narrow -> higher Q.
        const float q = 1.60f - 1.15f * (get(kBandwidth) * 0.01f);

        const BiquadCoeffs lb = BiquadCoeffs::lowShelf(sampleRate_, lf, 0.7071f, lowBoost);
        // The Pultec low boost/attenuation interaction is approximated by a
        // broad dip above the selected LF shelf, rather than a second shelf.
        const BiquadCoeffs ld = BiquadCoeffs::peak(sampleRate_, mpc::clamp(lf * 1.55f, 25.0f, 250.0f),
                                                   0.78f, -lowAtten);
        const BiquadCoeffs hb = BiquadCoeffs::peak(sampleRate_, hf, q, highBoost);
        const BiquadCoeffs ha = BiquadCoeffs::highShelf(sampleRate_, af, 0.7071f, -highAtten);
        for (Channel& c : channels_)
        {
            c.lowBoost.setCoeffs(lb); c.lowDip.setCoeffs(ld);
            c.highBoost.setCoeffs(hb); c.highAtten.setCoeffs(ha);
        }
    }

    double sampleRate_ = 44100.0;
    Channel channels_[2];
    Smoother output_, grain_, drive_, mix_;
};

MPC_PLUGIN_ENTRY(StrattPultek)
