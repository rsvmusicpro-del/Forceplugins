// StrattTrack Console7 input-stage numerical and recovery regression.
#include "dsp/airwindows_console7.h"
#include <cassert>
#include <cmath>
#include <limits>

int main() {
    const double rates[] = {8001.0, 44100.0, 48000.0, 96000.0, 192000.0, 384000.0};
    const float faders[] = {0.0f, 0.001f, 0.25f, 0.772f, 1.0f};
    for (double sr : rates) {
        for (float fader : faders) {
            stratt::dsp::Console7Channel channel;
            channel.prepare(sr);
            for (int i = 0; i < 200000; ++i) {
                const double phase = 6.283185307179586 * 997.0 * i / sr;
                const float input = static_cast<float>(0.95 * std::sin(phase)
                    + 0.025 * std::sin(6.283185307179586 * 73.0 * i / sr));
                const float output = channel.process(input, fader);
                assert(std::isfinite(output));
                assert(std::fabs(output) < 2.0f);
            }
            const float invalid[] = {std::numeric_limits<float>::quiet_NaN(),
                                     std::numeric_limits<float>::infinity(),
                                     -std::numeric_limits<float>::infinity(),
                                     std::numeric_limits<float>::max()};
            for (float x : invalid) assert(std::isfinite(channel.process(x, fader)));
            for (int i = 0; i < 4096; ++i) {
                const float y = channel.process(0.1f * std::sin(0.03f * i), fader);
                assert(std::isfinite(y));
                assert(std::fabs(y) < 2.0f);
            }
        }
    }
    return 0;
}
