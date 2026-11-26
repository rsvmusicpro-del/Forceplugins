// dsp/cleansweep_filter.h - stable, prewarped first-order analog HP/LP filters.
// The first-order prototype preserves the Cleansweep 6 dB/octave character.
// At first order, Butterworth and Bessel magnitude responses coincide; blending
// labels would add no meaningful analog curve and could only alter phase/gain.
// This clean-room implementation uses the bilinear transform with cutoff
// prewarping, preserving the analog -3 dB corner while keeping poles inside
// the unit circle. It is linear and time-invariant: no saturation or colour.
// SPDX-License-Identifier: MIT
#ifndef MPC_DSP_CLEANSWEEP_FILTER_H
#define MPC_DSP_CLEANSWEEP_FILTER_H

#include <cmath>

namespace mpc { namespace dsp {

class CleansweepOnePole
{
public:
    enum class Mode { Highpass, Lowpass };

    explicit CleansweepOnePole(Mode mode = Mode::Lowpass) : mode_(mode) {}

    // Prewarped bilinear transform of H_LP(s)=wc/(s+wc) or
    // H_HP(s)=s/(s+wc), with wc=2*pi*fc. Prewarping K=tan(pi*fc/fs)
    // makes the digital magnitude exactly 1/sqrt(2) at fc (up to float
    // coefficient quantisation), while the bilinear map preserves stability.
    void setCutoff(float hz, double sampleRate)
    {
        if (!std::isfinite(sampleRate) || sampleRate < 8000.0 || sampleRate > 384000.0)
            sampleRate = 44100.0;

        const double sr = sampleRate;
        double cutoff = std::isfinite(hz) ? static_cast<double>(hz) : 20.0;
        if (cutoff < 1.0) cutoff = 1.0;
        const double maximum = 0.49 * sr;
        if (cutoff > maximum) cutoff = maximum;

        const double k = std::tan(3.1415926535897932384626433832795 * cutoff / sr);
        const double inv = 1.0 / (1.0 + k);
        const double b0 = mode_ == Mode::Lowpass ? k * inv : inv;
        const double b1 = mode_ == Mode::Lowpass ? b0 : -b0;
        const double a1 = (k - 1.0) * inv;

        if (!std::isfinite(b0) || !std::isfinite(b1) || !std::isfinite(a1))
        {
            // Defensive fallback: a finite, unity-gain low-pass state.
            b0_ = 1.0f;
            b1_ = 0.0f;
            a1_ = 0.0f;
            reset();
            return;
        }

        b0_ = static_cast<float>(b0);
        b1_ = static_cast<float>(b1);
        a1_ = static_cast<float>(a1);
    }

    void reset() { x1_ = y1_ = 0.0f; }

    float process(float x)
    {
        // A corrupt host sample must not poison recursive state indefinitely.
        if (!std::isfinite(x))
        {
            reset();
            return 0.0f;
        }

        const float y = b0_ * x + b1_ * x1_ - a1_ * y1_;
        if (!std::isfinite(y))
        {
            reset();
            return 0.0f;
        }

        x1_ = x;
        y1_ = y;
        return y;
    }

private:
    Mode mode_;
    float b0_ = 1.0f, b1_ = 0.0f, a1_ = 0.0f;
    float x1_ = 0.0f, y1_ = 0.0f;
};

}} // namespace mpc::dsp
#endif
