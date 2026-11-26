#include "dsp/two_pole_filter.h"
#include <cmath>
#include <cstdio>
#include <limits>

using mpc::dsp::TwoPoleButterworth;

static double response(TwoPoleButterworth::Mode mode, double sr, double fc, double probe)
{
    TwoPoleButterworth filter(mode);
    filter.setCutoff(sr, static_cast<float>(fc));
    const int n = 65536, skip = 16384;
    double sum = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const float x = static_cast<float>(std::sin(6.2831853071795864769 * probe * i / sr));
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
    const double cutoffs[] = {100.0, 1000.0, 5000.0};

    for (double sr : rates)
    {
        for (double fc : cutoffs)
        {
            const double hp = response(TwoPoleButterworth::Mode::Highpass, sr, fc, fc);
            const double lp = response(TwoPoleButterworth::Mode::Lowpass, sr, fc, fc);
            if (check(std::fabs(hp - 0.70710678) < 0.015, "HP -3 dB corner mismatch", sr, fc)) return 1;
            if (check(std::fabs(lp - 0.70710678) < 0.015, "LP -3 dB corner mismatch", sr, fc)) return 2;

            if (fc * 4.0 >= 0.4 * sr) continue;
            const double hpLow = response(TwoPoleButterworth::Mode::Highpass, sr, fc, fc / 8.0);
            const double hpHigh = response(TwoPoleButterworth::Mode::Highpass, sr, fc, fc / 4.0);
            const double lpHigh1 = response(TwoPoleButterworth::Mode::Lowpass, sr, fc, fc * 2.0);
            const double lpHigh2 = response(TwoPoleButterworth::Mode::Lowpass, sr, fc, fc * 4.0);
            if (check(hpHigh / hpLow > 3.0 && hpHigh / hpLow < 5.2,
                      "HP slope is not approximately 12 dB/octave", sr, fc)) return 3;
            if (check(lpHigh1 / lpHigh2 > 3.0 && lpHigh1 / lpHigh2 < 5.2,
                      "LP slope is not approximately 12 dB/octave", sr, fc)) return 4;
        }
    }

    for (double sr : rates)
    {
        for (double fc : {20.0, 0.45 * sr})
        {
            for (TwoPoleButterworth::Mode mode : {TwoPoleButterworth::Mode::Highpass,
                                                   TwoPoleButterworth::Mode::Lowpass})
            {
                TwoPoleButterworth filter(mode);
                filter.setCutoff(sr, static_cast<float>(fc));
                for (int i = 0; i < 200000; ++i)
                {
                    const float x = (i & 1) ? 1.0f : -1.0f;
                    const float y = filter.process(x);
                    if (check(std::isfinite(y) && std::fabs(y) < 2.0f,
                              "Extreme-cutoff stability failure", sr, fc)) return 5;
                }
            }
        }
    }

    TwoPoleButterworth recovery(TwoPoleButterworth::Mode::Lowpass);
    recovery.setCutoff(44100.0, 1000.0f);
    if (check(recovery.process(std::numeric_limits<float>::quiet_NaN()) == 0.0f,
              "NaN input was not rejected", 44100.0, 1000.0)) return 6;
    if (check(std::isfinite(recovery.process(1.0f)),
              "Filter did not recover after NaN", 44100.0, 1000.0)) return 7;
    recovery.setCutoff(44100.0, std::numeric_limits<float>::infinity());
    if (check(std::isfinite(recovery.process(0.25f)),
              "Invalid cutoff destabilised filter", 44100.0, 1000.0)) return 8;

    std::puts("Shared Butterworth HP/LP corner, 12 dB/octave slope and stability checks passed");
    return 0;
}
