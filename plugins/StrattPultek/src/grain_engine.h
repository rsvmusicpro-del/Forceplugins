#pragma once

#include <cmath>

namespace strattpultek
{

// Level-independent adjacent-sample continuity detector for the Grain blend.
// Transients attack immediately; the detector recovers with a short one-pole
// release so the wet gain does not chatter at individual waveform crossings.
// All transcendental work is confined to prepare(); process() is allocation-
// free and uses only bounded arithmetic.
class GrainDensityDetector
{
public:
    void prepare(double sampleRate)
    {
        const double sr = (std::isfinite(sampleRate) && sampleRate >= 8000.0
                           && sampleRate <= 384000.0) ? sampleRate : 44100.0;
        releaseCoefficient_ = static_cast<float>(
            std::exp(-1.0 / (0.0005 * sr)));
        reset();
    }

    void reset()
    {
        state_ = 1.0f;
    }

    float process(float previousSample, float currentSample)
    {
        const float previousMagnitude = std::fabs(previousSample);
        const float currentMagnitude = std::fabs(currentSample);
        const float magnitudeSum = previousMagnitude + currentMagnitude;

        float continuity = 0.0f;
        const bool samePolarity =
            (previousSample > 0.0f && currentSample > 0.0f)
            || (previousSample < 0.0f && currentSample < 0.0f);

        if (samePolarity && magnitudeSum > 1.0e-12f)
        {
            const float smaller = previousMagnitude < currentMagnitude
                                    ? previousMagnitude : currentMagnitude;
            continuity = (2.0f * smaller) / magnitudeSum;
        }

        continuity = continuity * continuity * (3.0f - 2.0f * continuity);

        // Fast attack preserves sharp transients; a 0.5 ms release restores
        // colour smoothly between them. The coefficient is sample-rate aware.
        if (continuity < state_)
            state_ = continuity;
        else
            state_ += (continuity - state_) * (1.0f - releaseCoefficient_);

        return state_;
    }

private:
    float releaseCoefficient_ = 0.0f;
    float state_ = 1.0f;
};

} // namespace strattpultek
