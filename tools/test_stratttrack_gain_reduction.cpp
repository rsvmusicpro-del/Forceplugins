// Runtime regression for StrattTrack's live compressor gain-reduction telemetry.
// SPDX-License-Identifier: MIT
#include "../plugins/StrattTrack/src/StrattTrack.cpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {
constexpr int kFrames = 128;
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
    StrattTrack plugin;
    plugin.prepare(48000.0, kFrames);
    if (plugin.numParams() != 69 || plugin.param(kCompGR).automatable) {
        std::fprintf(stderr, "Comp GR must be the appended non-automatable parameter\n");
        return 1;
    }
    if (std::fabs(plugin.get(kCompGR)) > 1.0e-6f) {
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
