// dsp/two_pole_filter.h - shared second-order Butterworth HP/LP section.
// Identical transfer function for StrattTrack and StrattPultek.
// SPDX-License-Identifier: MIT
#ifndef MPC_DSP_TWO_POLE_FILTER_H
#define MPC_DSP_TWO_POLE_FILTER_H

#include <cmath>

namespace mpc { namespace dsp {

class TwoPoleButterworth
{
public:
    enum class Mode { Highpass, Lowpass };

    explicit TwoPoleButterworth(Mode mode = Mode::Lowpass) : mode_(mode) {}

    // RBJ/cookbook bilinear-transform Butterworth section, Q = 1/sqrt(2).
    // Coefficients are designed at control rate, never in the sample loop.
    void setCutoff(double sampleRate, float cutoffHz)
    {
        if (!std::isfinite(sampleRate) || sampleRate < 8000.0 || sampleRate > 384000.0)
            sampleRate = 44100.0;

        const double sr = sampleRate;
        double fc = std::isfinite(cutoffHz) ? static_cast<double>(cutoffHz) : 20.0;
        if (fc < 1.0) fc = 1.0;
        const double maximum = 0.45 * sr;
        if (fc > maximum) fc = maximum;

        const double omega = 6.283185307179586476925286766559 * fc / sr;
        const double cosine = std::cos(omega);
        const double sine = std::sin(omega);
        const double alpha = sine / (2.0 * 0.707106781186547524400844362105);

        const double a0 = 1.0 + alpha;
        const double a1 = -2.0 * cosine;
        const double a2 = 1.0 - alpha;
        double b0, b1, b2;

        if (mode_ == Mode::Highpass)
        {
            b0 = (1.0 + cosine) * 0.5;
            b1 = -(1.0 + cosine);
            b2 = b0;
        }
        else
        {
            b0 = (1.0 - cosine) * 0.5;
            b1 = 1.0 - cosine;
            b2 = b0;
        }

        const double invA0 = 1.0 / a0;
        const float nb0 = static_cast<float>(b0 * invA0);
        const float nb1 = static_cast<float>(b1 * invA0);
        const float nb2 = static_cast<float>(b2 * invA0);
        const float na1 = static_cast<float>(a1 * invA0);
        const float na2 = static_cast<float>(a2 * invA0);
        if (!std::isfinite(nb0) || !std::isfinite(nb1) || !std::isfinite(nb2)
            || !std::isfinite(na1) || !std::isfinite(na2))
            return;

        b0_ = nb0; b1_ = nb1; b2_ = nb2; a1_ = na1; a2_ = na2;
    }

    void reset() { z1_ = z2_ = 0.0f; }

    float process(float x)
    {
        if (!std::isfinite(x))
        {
            reset();
            return 0.0f;
        }

        const float y = b0_ * x + z1_;
        const float z1 = b1_ * x - a1_ * y + z2_;
        const float z2 = b2_ * x - a2_ * y;
        if (!std::isfinite(y) || !std::isfinite(z1) || !std::isfinite(z2))
        {
            reset();
            return 0.0f;
        }
        z1_ = z1;
        z2_ = z2;
        return y;
    }

private:
    Mode mode_;
    float b0_ = 1.0f, b1_ = 0.0f, b2_ = 0.0f;
    float a1_ = 0.0f, a2_ = 0.0f;
    float z1_ = 0.0f, z2_ = 0.0f;
};

}} // namespace mpc::dsp
#endif
