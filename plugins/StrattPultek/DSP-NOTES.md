# StrattPultek DSP notes and verification plan

## EQ control coverage

| Section | Choices / range | DSP stage |
|---|---|---|
| LF frequency | 20, 30, 60, 100 Hz | Selects both LF shelves; attenuation corner is offset above the boost corner |
| LF boost | 0–10 dB | Low-frequency shelving boost at the selected frequency |
| LF attenuation | 0–10 dB | Low-frequency shelving cut at 1.414 × selected frequency (half-octave corner offset); cascades with boost to create the LF lift/dip interaction |
| HF boost frequency | 3, 4, 5, 8, 10, 12, 16 kHz | Peaking bell |
| HF boost | 0–10 dB | Peaking bell gain |
| Bandwidth | 0–100% | Q from 1.60 (narrow) to 0.45 (broad) |
| HF attenuation frequency | 5, 10, 20 kHz | High shelf |
| HF attenuation | 0–10 dB | High shelf cut |
| Grain / Drive | 0–100% each | C1 polynomial soft clip with first-order ADAA plus a small asymmetric even-harmonic term; Drive sets saturation intensity, Grain sets its blend; both zero are neutral |
| Output / Mix | -12…+12 dB / 0–100% | Output trim and parallel blend |

These are EQP-1A-inspired operating ranges and a digital curve approximation. Both LF controls are implemented as shelves, with the attenuation corner offset higher than the boost corner; this follows the documented passive-filter topology and produces the characteristic combined lift/dip. The 1.414 corner ratio is a practical half-octave approximation, not a measured plot of a particular hardware serial number. Different Pultec revisions and component tolerances differ; exact reproduction requires a selected reference unit and measured sweeps.

## CPU design

- Four biquad sections per channel (LF boost shelf, offset LF attenuation shelf, HF bell boost, HF shelf attenuation); coefficients computed once per process block.
- All eight EQ controls use a 15 ms block-size-compensated glide before coefficient recalculation, avoiding abrupt filter jumps while keeping the coefficient work out of the sample loop.
- One-pole 15 ms smoothing for output, grain, drive and mix.
- The colour stage uses first-order antiderivative antialiasing (ADAA). Its polynomial clip and squared-clip asymmetric component both have closed-form integrals, so the divided difference adds controlled even-order harmonics while reducing high-frequency foldback without per-sample log/exp/tanh or oversampling. A 3 Hz one-pole DC blocker removes the asymmetry's bias. Drive at 0% bypasses its contribution and Grain at 0% is fully clean.
- The ADAA and DC-blocker memories are per channel and reset with the filter state; the previous nonlinear input is not shared between stereo channels.
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
3. Frequency sweeps at all LF/HF selections; verify finite output, the combined LF shelf lift/dip, HF bell/shelf directions and no instability.
4. Compare level-matched response plots with the published EQP-1A curves, then tune the LF shelf corner ratio and gain curves, HF Q using a serviced reference unit. Do not label the response hardware-matched until that measurement exists.
5. Measure CPU on Force hardware; host_sim desktop timing is not a substitute for ARM timing.
6. Level-matched listening against a serviced EQP-1A reference before describing the response as hardware-matched.
