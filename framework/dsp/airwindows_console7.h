// Airwindows Console7Channel-inspired input console stage.
// Original Console7Channel transfer/topology by Chris Johnson (Airwindows).
// Copyright (c) 2016 Chris Johnson. Airwindows source is MIT licensed.
// This adaptation retains the 20 kHz / Q=1.618 input low-pass, Fader law,
// Spiral/Density 80:20 transfer and gain compensation. The static transfer is
// evaluated as a convergent polynomial and antiderivative ADAA to reduce
// foldback; it is not bit-identical to the original non-antialiased VST.
// MIT License: https://github.com/airwindows/airwindows/blob/master/LICENSE
#pragma once

#include <algorithm>
#include <cmath>

namespace stratt { namespace dsp {

class Console7Channel
{
public:
    void prepare(double sampleRate)
    {
        const double sr = (std::isfinite(sampleRate) && sampleRate > 8000.0)
                        ? sampleRate : 44100.0;
        const double frequency = std::min(20000.0, sr * 0.45);
        constexpr double q = 1.6180339887498948482;
        const double k = std::tan(3.14159265358979323846 * frequency / sr);
        const double norm = 1.0 / (1.0 + k / q + k * k);
        b0_ = k * k * norm;
        b1_ = 2.0 * b0_;
        b2_ = b0_;
        a1_ = 2.0 * (k * k - 1.0) * norm;
        a2_ = (1.0 - k / q + k * k) * norm;
        reset();
    }

    void reset()
    {
        z1_ = z2_ = 0.0;
        previous_ = 0.0;
        hasPrevious_ = false;
    }

    float process(float input, float fader) 
    {
        const double x = std::isfinite(input) ? std::max(-16.0, std::min(16.0, static_cast<double>(input))) : 0.0;
        const double filtered = b0_ * x + z1_;
        z1_ = b1_ * x - a1_ * filtered + z2_;
        z2_ = b2_ * x - a2_ * filtered;
        if (!std::isfinite(filtered) || !std::isfinite(z1_) || !std::isfinite(z2_))
        {
            reset();
            return 0.0f;
        }

        const double amount = std::isfinite(fader) ? std::max(0.0, std::min(1.0, static_cast<double>(fader))) : 0.772;
        const double gain = amount * 1.272019649514069;
        if (gain <= 1.0e-8)
        {
            previous_ = 0.0;
            hasPrevious_ = false;
            return 0.0f;
        }

        // Airwindows Console7Channel's gain staging: gain^3 into the
        // nonlinear channel, then divide by gain after the transfer.
        const double driven = std::max(-1.097, std::min(1.097, filtered * gain * gain * gain));
        const double shaped = hasPrevious_ ? adaa(driven, previous_) : transfer(driven);
        previous_ = driven;
        hasPrevious_ = true;
        const double output = shaped / gain;
        return std::isfinite(output) ? static_cast<float>(output) : 0.0f;
    }

private:
    // Taylor coefficients of 0.8*sin(x*abs(x))/abs(x) + 0.2*sin(x),
    // evaluated on |x| <= 1.097. Terms through x^21 keep the maximum
    // truncation error below 1.5e-7 over the complete clipped domain.
    static double coefficient(int i)
    {
        static const double c[11] = {
            1.0, -0.0333333333333333333, -0.131666666666666667,
            -0.0000396825396825396825, 0.006667217813051146,
            -0.000000005010421677088344, -0.00015873012661207105,
            -0.0000000000001529432746363963, 0.000002204585538481163,
            0.0, -0.000000020041686708353376
        };
        return c[i];
    }

    static double polynomial(double x)
    {
        const double z = x * x;
        double p = coefficient(10);
        for (int i = 9; i >= 0; --i) p = coefficient(i) + z * p;
        return x * p;
    }

    static double transfer(double x)
    {
        constexpr double limit = 1.097;
        const double a = std::fabs(x);
        return a <= limit ? polynomial(x) : std::copysign(polynomial(limit), x);
    }

    static double primitive(double x)
    {
        constexpr double limit = 1.097;
        const double a = std::fabs(x);
        const double z = std::min(a, limit) * std::min(a, limit);
        double p = coefficient(10) / 22.0;
        for (int i = 9; i >= 0; --i)
            p = coefficient(i) / static_cast<double>(2 * i + 2) + z * p;
        const double atLimit = z * p;
        return a <= limit ? atLimit
                          : atLimit + polynomial(limit) * (a - limit);
    }

    static double adaa(double x, double previous)
    {
        const double delta = x - previous;
        const double scale = std::max(1.0, std::max(std::fabs(x), std::fabs(previous)));
        if (std::fabs(delta) <= 1.0e-5 * scale)
            return transfer(0.5 * (x + previous));
        return (primitive(x) - primitive(previous)) / delta;
    }

    double b0_ = 1.0, b1_ = 0.0, b2_ = 0.0, a1_ = 0.0, a2_ = 0.0;
    double z1_ = 0.0, z2_ = 0.0;
    double previous_ = 0.0;
    bool hasPrevious_ = false;
};

}} // namespace stratt::dsp

// Airwindows Console7Channel was released under the MIT License. The original
// copyright and license notice are retained here; this adaptation also uses
// MIT terms. See the linked upstream LICENSE for the complete permission text.
