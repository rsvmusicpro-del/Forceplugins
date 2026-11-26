#include "grain_engine.h"

#include <cmath>
#include <cstdio>

using strattpultek::GrainDensityDetector;

static int fail(const char* message)
{
    std::fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

static double meanSineDensity(double frequency, double sampleRate, double amplitude)
{
    GrainDensityDetector detector;
    detector.prepare(sampleRate);
    const int frames = static_cast<int>(sampleRate * 0.25);
    double total = 0.0;
    float previous = 0.0f;
    for (int i = 0; i < frames; ++i)
    {
        const float current = amplitude * static_cast<float>(
            std::sin(6.2831853071795864769 * frequency * i / sampleRate));
        total += detector.process(previous, current);
        previous = current;
    }
    return total / frames;
}

int main()
{
    GrainDensityDetector positive;
    positive.prepare(48000.0);
    if (std::fabs(positive.process(0.4f, 0.4f) - 1.0f) > 1.0e-6f)
        return fail("steady positive waveform must retain full density");

    GrainDensityDetector negative;
    negative.prepare(48000.0);
    if (std::fabs(negative.process(-0.4f, -0.4f) - 1.0f) > 1.0e-6f)
        return fail("steady negative waveform must retain full density");

    GrainDensityDetector nominal, quiet;
    nominal.prepare(48000.0);
    quiet.prepare(48000.0);
    const float nominalWeight = nominal.process(0.4f, 0.6f);
    const float quietWeight = quiet.process(0.004f, 0.006f);
    if (std::fabs(nominalWeight - quietWeight) > 1.0e-6f)
        return fail("density must be independent of absolute signal level");

    GrainDensityDetector transient;
    transient.prepare(48000.0);
    transient.process(0.5f, 0.5f);
    const float signCrossing = transient.process(0.5f, -0.5f);
    if (signCrossing != 0.0f)
        return fail("polarity reversal must trigger immediate transient attenuation");
    const float step = transient.process(0.01f, 0.9f);
    if (!(step >= 0.0f && step < 0.01f))
        return fail("large same-polarity amplitude step must attenuate Grain");

    GrainDensityDetector recovery;
    recovery.prepare(48000.0);
    recovery.process(0.5f, 0.5f);
    if (recovery.process(0.5f, -0.5f) != 0.0f)
        return fail("transient attack must be immediate");
    float previous = 0.0f;
    float current = 0.0f;
    for (int i = 0; i < 100; ++i)
    {
        current = recovery.process(0.5f, 0.5f);
        if (current + 1.0e-7f < previous)
            return fail("release must recover monotonically");
        previous = current;
    }
    if (current < 0.98f)
        return fail("0.5 ms release must recover within the expected time");

    GrainDensityDetector silence;
    silence.prepare(44100.0);
    for (int i = 0; i < 1000; ++i)
    {
        const float value = silence.process(0.0f, 0.0f);
        if (!std::isfinite(value) || value != 0.0f)
            return fail("silence must remain finite and produce zero density");
    }

    GrainDensityDetector bounds;
    bounds.prepare(96000.0);
    for (int i = 0; i < 10000; ++i)
    {
        const float a = (i & 1) ? 1.5f : -1.5f;
        const float b = (i % 3) ? 0.75f : -0.75f;
        const float value = bounds.process(a, b);
        if (!std::isfinite(value) || value < 0.0f || value > 1.0f)
            return fail("detector output must remain finite and bounded");
    }

    const double lowFrequencyDensity = meanSineDensity(100.0, 48000.0, 0.5);
    const double highFrequencyDensity = meanSineDensity(4000.0, 48000.0, 0.5);
    if (!(lowFrequencyDensity > highFrequencyDensity + 0.5))
        return fail("higher-frequency waveform must receive less saturation density");

    std::puts("Grain continuity, level invariance, transient attack, release, frequency response and bounds passed");
    return 0;
}
