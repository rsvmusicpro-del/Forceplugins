// Runtime regression for StrattTrack's live compressor gain-reduction telemetry.
// SPDX-License-Identifier: MIT
#include "../plugins/StrattTrack/src/StrattTrack.cpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {
constexpr int kFrames = 128;
double responseDb(const Biquad& filter, double sampleRate, double frequency)
{
    const double w = 6.28318530717958647692 * frequency / sampleRate;
    const double c1 = std::cos(w), s1 = std::sin(w);
    const double c2 = std::cos(2.0*w), s2 = std::sin(2.0*w);
    const double nr = filter.b0 + filter.b1*c1 + filter.b2*c2;
    const double ni = -filter.b1*s1 - filter.b2*s2;
    const double dr = 1.0 + filter.a1*c1 + filter.a2*c2;
    const double di = -filter.a1*s1 - filter.a2*s2;
    const double magnitude = std::sqrt((nr*nr+ni*ni)/(dr*dr+di*di));
    return 20.0*std::log10(magnitude);
}
bool nearDb(double actual, double expected)
{
    return std::isfinite(actual) && std::fabs(actual-expected) < 0.02;
}
void render(StrattTrack& plugin, float amplitude, int blocks, double& phase)
{
    float inL[kFrames], inR[kFrames], outL[kFrames], outR[kFrames];
    const float* inputs[2] = {inL, inR};
    float* outputs[2] = {outL, outR};
    constexpr double twoPi = 6.28318530717958647692;
    constexpr double frequency = 997.0;
    for (int block = 0; block < blocks; ++block) {
        for (int i = 0; i < kFrames; ++i) {
            const float sample = amplitude * static_cast<float>(std::sin(phase));
            inL[i] = inR[i] = sample;
            phase += twoPi * frequency / 48000.0;
            if (phase >= twoPi) phase -= twoPi;
        }
        mpc::ProcessContext context;
        context.inputs = inputs;
        context.outputs = outputs;
        context.numInputs = 2;
        context.numOutputs = 2;
        context.numFrames = kFrames;
        context.sampleRate = 48000.0;
        plugin.process(context);
        for (int i = 0; i < kFrames; ++i) {
            if (!std::isfinite(outL[i]) || !std::isfinite(outR[i])) {
                std::fprintf(stderr, "non-finite output at block %d frame %d\n", block, i);
                std::exit(1);
            }
        }
    }
}
}

int main()
{
    // Independently verify the four requested RBJ contours at their design
    // frequencies / shelf plateaus before exercising the plugin as a whole.
    Biquad warmLow, warmHigh, clarityLow, clarityPresence, neutral;
    warmLow.shelf(48000.0f,180.0f,1.5f,false);
    warmHigh.shelf(48000.0f,10000.0f,-1.25f,true);
    clarityLow.bell(48000.0f,280.0f,0.75f,-1.5f);
    clarityPresence.bell(48000.0f,3200.0f,0.80f,1.75f);
    neutral.shelf(48000.0f,180.0f,0.0f,false);
    if (!nearDb(responseDb(warmLow,48000.0,0.0),1.5) ||
        !nearDb(responseDb(warmHigh,48000.0,24000.0),-1.25) ||
        !nearDb(responseDb(clarityLow,48000.0,280.0),-1.5) ||
        !nearDb(responseDb(clarityPresence,48000.0,3200.0),1.75) ||
        !nearDb(responseDb(neutral,48000.0,0.0),0.0)) {
        std::fprintf(stderr, "Warmth/Clarity RBJ response does not match its specified contour\n");
        return 1;
    }
    StrattTrack plugin;
    plugin.prepare(48000.0, kFrames);
    if (plugin.numParams() != 71 || plugin.param(kCompGR).automatable || !plugin.param(kCompGR).transient) {
        std::fprintf(stderr, "Comp GR must remain non-automatable transient telemetry\n");
        return 1;
    }
    if (plugin.get(kToneWarmth) != 0.0f || plugin.get(kToneClarity) != 0.0f) {
        std::fprintf(stderr, "Warmth and Clarity must default to neutral\n");
        return 1;
    }\n    if (std::fabs(plugin.get(kCompGR)) > 1.0e-6f) {
        std::fprintf(stderr, "Comp GR must reset to 0 dB\n");
        return 1;
    }
    plugin.set(kCompThresh, -24.0f);
    plugin.set(kCompRatio, 4.0f);
    plugin.set(kCompAttack, 0.1f);
    plugin.set(kCompRelease, 100.0f);
    plugin.set(kGateThresh, -80.0f);
    plugin.set(kPreamp, 0.0f);
    double phase = 0.0;
    render(plugin, 0.01f, 100, phase);
    if (plugin.get(kCompGR) > 0.1f) {
        std::fprintf(stderr, "below-threshold signal must show no compressor gain reduction: %.3f dB\n", plugin.get(kCompGR));
        return 1;
    }
    render(plugin, 0.8f, 400, phase);
    const float compressed = plugin.get(kCompGR);
    if (!std::isfinite(compressed) || compressed < 2.0f || compressed > 60.0f) {
        std::fprintf(stderr, "loud signal must show bounded positive gain reduction: %.3f dB\n", compressed);
        return 1;
    }
    const std::string display = plugin.displayText(kCompGR);
    if (display.find("dB") == std::string::npos) {
        std::fprintf(stderr, "Comp GR display must include dB units: %s\n", display.c_str());
        return 1;
    }
    render(plugin, 0.0f, 500, phase);
    if (plugin.get(kCompGR) > 0.2f) {
        std::fprintf(stderr, "compressor release must return Comp GR near 0 dB: %.3f dB\n", plugin.get(kCompGR));
        return 1;
    }
    plugin.reset();
    if (std::fabs(plugin.get(kCompGR)) > 1.0e-6f) {
        std::fprintf(stderr, "Comp GR reset must clear telemetry\n");
        return 1;
    }
    std::puts("StrattTrack live compressor gain-reduction telemetry OK");
    return 0;
}
