# StrattPultek DSP notes and verification plan

## EQ control coverage

| Section | Choices / range | DSP stage |
|---|---|---|
| LF frequency | 20, 30, 60, 100 Hz | Selects LF shelf and LF attenuation interaction centre |
| LF boost | 0–10 dB | Low shelf |
| LF attenuation | 0–10 dB | Broad bell dip at 1.55 × selected LF frequency |
| HF boost frequency | 3, 4, 5, 8, 10, 12, 16 kHz | Peaking bell |
| HF boost | 0–10 dB | Peaking bell gain |
| Bandwidth | 0–100% | Q from 1.60 (narrow) to 0.45 (broad) |
| HF attenuation frequency | 5, 10, 20 kHz | High shelf |
| HF attenuation | 0–10 dB | High shelf cut |
| Grain / Drive | 0–100% each | C1 polynomial soft clip with first-order ADAA; Drive sets saturation intensity, Grain sets its blend; both zero are neutral |
| Output / Mix | -12…+12 dB / 0–100% | Output trim and parallel blend |

These are EQP-1A-inspired operating ranges and a digital curve approximation. They are not asserted as measured plots of a particular hardware serial number. Different Pultec revisions and component tolerances differ; exact reproduction requires a selected reference unit and measured sweeps.

## CPU design

- Four biquad sections per channel; coefficients computed once per process block.
- All eight EQ controls use a 15 ms block-size-compensated glide before coefficient recalculation, avoiding abrupt filter jumps while keeping the coefficient work out of the sample loop.
- One-pole 15 ms smoothing for output, grain, drive and mix.
- The colour stage uses first-order antiderivative antialiasing (ADAA). Its polynomial clip has a closed-form integral, so the divided difference reduces high-frequency foldback without per-sample log/exp/tanh or oversampling. Drive at 0% bypasses its contribution and Grain at 0% is fully clean.
- The ADAA memory is per channel and reset with the filter state; the previous nonlinear input is not shared between stereo channels.
- Frequency selectors use logarithmic 15 ms smoothing; gain and bandwidth controls retain the 15 ms block-size-compensated glide.
- No oversampling in this low-CPU baseline. ADAA reduces, but does not eliminate, aliasing. Compare spectral sweeps at high Drive against the previous direct waveshaper and measure ARM CPU before considering 2x/4x oversampling.
- No heap allocation, locks, logging or I/O in the audio callback.

## Q-Link mapping

Normal: Q-Link 1 Low Boost (parameter 2), Q-Link 2 HF Boost (5), Q-Link 3 Grain (9).
SHIFT: Q-Link 1 Low Atten (3), Q-Link 2 HF Atten (8), Q-Link 3 Mix (11).
The first parameter remains Output trim, intentionally harmless under the MPC prepare-time parameter-0 nudge. Parameter order and IDs are stable.

## Required bench checks

1. SDK build for native, ARM32 and ARM64; generated UI and tools/validate.py must pass.
2. host_sim test at 44.1, 48 and 96 kHz with 32-, 128- and 512-frame blocks; the simulator exercises parameter changes, state round-trip, in-place processing and silence pass-through.
3. Frequency sweeps at all LF/HF selections; verify finite output and expected shelf/peak/dip directions and no instability.
4. Measure CPU on Force hardware; host_sim desktop timing is not a substitute for ARM timing.
5. Level-matched listening against a serviced EQP-1A reference before describing the response as hardware-matched.
