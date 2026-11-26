#include "dsp/cleansweep_filter.h"
#include "dsp/two_pole_filter.h"
#include "dsp/biquad.h"
#include <cmath>
#include <cstdio>
#include <limits>
#include <initializer_list>

using mpc::dsp::CleansweepOnePole;
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


    for (double sr : rates)
    {
        for (float q : {0.45f, 0.9f, 1.6f})
        {
            mpc::dsp::Biquad f;
            f.setCoeffs(mpc::dsp::BiquadCoeffs::resonantBoost(sr, 8000.0f, q, 12.0f));
            double sum = 0.0; const int n = 65536, skip = 16384;
            for (int i = 0; i < n; ++i) {
                const float x = static_cast<float>(std::sin(6.2831853071795864769 * 8000.0 * i / sr));
                const float y = f.process(x);
                if (!std::isfinite(y)) return 9;
                if (i >= skip) sum += static_cast<double>(y) * y;
            }
            const double centre = std::sqrt(sum / (n - skip)) * std::sqrt(2.0);
            if (check(std::fabs(centre - std::pow(10.0, 12.0 / 20.0)) < 0.035,
                      "RLC resonant centre gain mismatch", sr, 8000.0)) return 10;
        }
    }

    // Retain the first-order Cleansweep tests from main as well as the
    // second-order shared Butterworth and resonant-RLC coverage above.
    const double onePoleCutoffs[] = {20.0, 100.0, 1000.0, 5000.0};
    for (double sr : rates)
    {
        for (double fc : onePoleCutoffs)
        {
            if (fc >= 0.2 * sr) continue;
            const double hp = response(CleansweepOnePole::Mode::Highpass, sr, fc, fc);
            const double lp = response(CleansweepOnePole::Mode::Lowpass, sr, fc, fc);
            if (check(std::fabs(hp - 0.70710678) < 0.012, "One-pole HP -3 dB corner mismatch", sr, fc)) return 11;
            if (check(std::fabs(lp - 0.70710678) < 0.012, "One-pole LP -3 dB corner mismatch", sr, fc)) return 12;

            if (fc < 1000.0) continue;
            const double hpLow = response(CleansweepOnePole::Mode::Highpass, sr, fc, fc / 8.0);
            const double hpHigh = response(CleansweepOnePole::Mode::Highpass, sr, fc, fc / 4.0);
            const double lpHigh1 = response(CleansweepOnePole::Mode::Lowpass, sr, fc, fc * 2.0);
            const double lpHigh2 = response(CleansweepOnePole::Mode::Lowpass, sr, fc, fc * 4.0);
            if (check(hpLow < hpHigh && hpHigh / hpLow > 1.75, "One-pole HP slope is not approximately 6 dB/octave", sr, fc)) return 13;
            if (check(lpHigh2 < lpHigh1 && lpHigh1 / lpHigh2 > 1.65, "One-pole LP slope is not approximately 6 dB/octave", sr, fc)) return 14;
        }
    }

    for (double sr : rates)
    {
        for (double fc : {20.0, 0.49 * sr})
        {
            for (CleansweepOnePole::Mode mode : {CleansweepOnePole::Mode::Highpass, CleansweepOnePole::Mode::Lowpass})
            {
                CleansweepOnePole filter(mode);
                filter.setCutoff(static_cast<float>(fc), sr);
                for (int i = 0; i < 200000; ++i)
                {
                    const float x = (i & 1) ? 1.0f : -1.0f;
                    const float y = filter.process(x);
                    if (check(std::isfinite(y) && std::fabs(y) < 2.0f, "One-pole extreme-cutoff stability failure", sr, fc)) return 15;
                }
            }
        }
    }

    CleansweepOnePole onePoleRecovery(CleansweepOnePole::Mode::Lowpass);
    onePoleRecovery.setCutoff(1000.0f, 44100.0);
    if (check(onePoleRecovery.process(std::numeric_limits<float>::quiet_NaN()) == 0.0f,
              "One-pole NaN input was not rejected", 44100.0, 1000.0)) return 16;
    if (check(std::isfinite(onePoleRecovery.process(1.0f)), "One-pole filter did not recover after NaN", 44100.0, 1000.0)) return 17;
    onePoleRecovery.setCutoff(std::numeric_limits<float>::infinity(), 44100.0);
    if (check(std::isfinite(onePoleRecovery.process(0.25f)), "One-pole invalid cutoff destabilised filter", 44100.0, 1000.0)) return 18;

    std::puts("Shared Cleansweep one-pole, Butterworth and resonant-RLC response checks passed");
    return 0;
}
