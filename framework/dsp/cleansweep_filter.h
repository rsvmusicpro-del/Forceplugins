// dsp/cleansweep_filter.h - stable first-order bilinear HP/LP filters.
// bx_cleansweep V2 is documented as a pair of musical 6 dB/octave filters.
// This clean-room implementation reproduces that documented topology and
// cutoff ranges; it does not claim to reproduce Brainworx's proprietary
// Anti-Crush circuit/curve algorithm.
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
    void setCutoff(float hz, double sampleRate)
    {
        if (!std::isfinite(sampleRate) || sampleRate < 8000.0) sampleRate = 44100.0;
        const float sr = static_cast<float>(sampleRate);
        if (!std::isfinite(hz)) hz = 20.0f;
        hz = hz < 1.0f ? 1.0f : (hz > 0.49f * sr ? 0.49f * sr : hz);
        const float k = std::tan(3.14159265358979323846f * hz / sr);
        const float inv = 1.0f / (1.0f + k);
        if (mode_ == Mode::Lowpass) { b0_ = k * inv; b1_ = b0_; }
        else { b0_ = inv; b1_ = -inv; }
        a1_ = (k - 1.0f) * inv;
    }
    void reset() { x1_ = y1_ = 0.0f; }
    float process(float x)
    {
        const float y = b0_ * x + b1_ * x1_ - a1_ * y1_;
        x1_ = x; y1_ = y;
        return y;
    }
private:
    Mode mode_;
    float b0_ = 1.0f, b1_ = 0.0f, a1_ = 0.0f;
    float x1_ = 0.0f, y1_ = 0.0f;
};
}} // namespace mpc::dsp
#endif
