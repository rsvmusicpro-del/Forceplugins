#include "dsp/cleansweep_filter.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>

using mpc::dsp::CleansweepOnePole;

static double response(CleansweepOnePole::Mode mode, double sr, double fc, double probe)
{
    CleansweepOnePole filter(mode);
    filter.setCutoff(static_cast<float>(fc), sr);
    const int n = 65536, skip = 16384;
    double sum = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const float x = static_cast<float>(std::sin(2.0 * 3.14159265358979323846 * probe * i / sr));
        const float y = filter.process(x);
        if (!std::isfinite(y)) return std::numeric_limits<double>::quiet_NaN();
        if (i >= skip) sum += static_cast<double>(y) * y;
    }
    return std::sqrt(sum / (n - skip)) * std::sqrt(2.0);
}

static int check(bool condition, const char* message, double sr, double fc)
{
    if (condition) return 0;
    std::fprintf(stderr, "%s (sr=%.0f fc=%.2f)\n", message, sr, fc);
    return 1;
}

int main()
{
    const double rates[] = {44100.0, 48000.0, 96000.0, 192000.0};
    const double cutoffs[] = {20.0, 100.0, 1000.0, 5000.0};

    for (double sr : rates)
    {
        for (double fc : cutoffs)
        {
            if (fc >= 0.2 * sr) continue;
            const double hp = response(CleansweepOnePole::Mode::Highpass, sr, fc, fc);
            const double lp = response(CleansweepOnePole::Mode::Lowpass, sr, fc, fc);
            if (check(std::fabs(hp - 0.70710678) < 0.012, "HP -3 dB corner mismatch", sr, fc)) return 1;
            if (check(std::fabs(lp - 0.70710678) < 0.012, "LP -3 dB corner mismatch", sr, fc)) return 2;

            if (fc < 1000.0) continue; // Avoid sub-cycle RMS estimates at low probe frequencies.\n            const double hpLow = response(CleansweepOnePole::Mode::Highpass, sr, fc, fc / 8.0);
            const double hpHigh = response(CleansweepOnePole::Mode::Highpass, sr, fc, fc / 4.0);
            const double lpHigh1 = response(CleansweepOnePole::Mode::Lowpass, sr, fc, fc * 2.0);
            const double lpHigh2 = response(CleansweepOnePole::Mode::Lowpass, sr, fc, fc * 4.0);
            if (check(hpLow < hpHigh && hpHigh / hpLow > 1.75, "HP slope is not approximately 6 dB/octave", sr, fc)) return 3;
            if (check(lpHigh2 < lpHigh1 && lpHigh1 / lpHigh2 > 1.65, "LP slope is not approximately 6 dB/octave", sr, fc)) return 4;
        }
    }

    // DC/passband checks and long-run boundedness at the lowest/highest cutoffs.
    for (double sr : rates)
    {
        for (double fc : {20.0, 0.49 * sr})
        {
            for (CleansweepOnePole::Mode mode : {CleansweepOnePole::Mode::Highpass,
                                                 CleansweepOnePole::Mode::Lowpass})
            {
                CleansweepOnePole f(mode);
                f.setCutoff(static_cast<float>(fc), sr);
                for (int i = 0; i < 200000; ++i)
                {
                    const float x = (i & 1) ? 1.0f : -1.0f;
                    const float y = f.process(x);
                    if (check(std::isfinite(y) && std::fabs(y) < 2.0f, "Extreme-cutoff stability failure", sr, fc)) return 5;
                }
            }
        }
    }

    // Non-finite input must reset recursive memory rather than poison later audio.
    CleansweepOnePole recovery(CleansweepOnePole::Mode::Lowpass);
    recovery.setCutoff(1000.0f, 44100.0);
    if (check(recovery.process(std::numeric_limits<float>::quiet_NaN()) == 0.0f,
              "NaN input was not rejected", 44100.0, 1000.0)) return 6;
    if (check(std::isfinite(recovery.process(1.0f)), "Filter did not recover after NaN", 44100.0, 1000.0)) return 7;
    recovery.setCutoff(std::numeric_limits<float>::infinity(), 44100.0);
    if (check(std::isfinite(recovery.process(0.25f)), "Invalid cutoff destabilised filter", 44100.0, 1000.0)) return 8;

    std::puts("Cleansweep analog-prototype, -3 dB calibration, 6 dB/octave slope and stability checks passed");
    return 0;
}
