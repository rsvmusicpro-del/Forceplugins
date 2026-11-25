# StrattPultek 1.2.0 DSP notes and verification plan

## EQ control coverage

| Section | Choices / range | DSP stage |
|---|---|---|
| LF frequency | 20, 30, 60, 100 Hz | Selects both LF shelves; attenuation corner is offset above the boost corner |
| LF boost | 0–14 dB | First-order low-frequency shelf boost at the selected frequency (6 dB/oct) |
| LF attenuation | 0–18 dB | First-order low-frequency shelf cut at 1.414 × selected frequency (half-octave corner offset); cascades with boost to create the LF lift/dip interaction |
| HF boost frequency | 3, 4, 5, 8, 10, 12, 16 kHz | Peaking bell |
| HF boost | 0–20 dB | Peaking bell gain, scaled from +20 dB narrow to about +13.5 dB broad |
| Bandwidth | 0–100% | Q from 1.60 (narrow) to 0.45 (broad) |
| HF attenuation frequency | 5, 10, 20 kHz | Selector maps to the approximate mid-attenuation point by solving the discrete-time one-pole response at the current sample rate |
| HF attenuation | 0–20 dB | First-order high shelf cut (6 dB/oct) |
| Grain / Drive | 0–100% each | C1 polynomial soft clip with first-order ADAA plus a small asymmetric even-harmonic term; Drive sets saturation intensity, Grain sets its blend; both zero are neutral |
| Output / Mix | -12…+12 dB / 0–100% | Output trim and parallel blend |

These are EQP-1A-inspired operating ranges and a digital curve approximation. The LF boost and attenuation and HF attenuation are implemented as first-order 6 dB/octave shelves, matching the documented passive-filter topology; the LF attenuation corner is offset higher than the boost corner and produces the characteristic combined lift/dip. The 1.414 LF corner ratio is a practical half-octave approximation, not a measured plot of a particular hardware serial number. The HF attenuation selector is mapped to the documented mid-attenuation point (about -11.4 dB for a -20 dB maximum) by solving the discrete-time one-pole shelf response for each selected frequency and sample rate. This avoids the large error a fixed analog frequency ratio would create near Nyquist at 20 kHz / 44.1 kHz. The HF boost gain is reduced linearly with the broadness control from +20 dB at the narrow end to +13.5 dB at the broad end; both are practical approximations of published Manley EQP-1A behaviour, not a component-level capture. Different Pultec revisions and component tolerances differ; exact reproduction requires a selected reference unit and measured sweeps.

## CPU design

- One biquad (HF bell) and three first-order shelves per channel (LF boost, offset LF attenuation, HF attenuation); coefficients are updated once per process block.
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

## Public reference material

- [Pulse Techniques — EQP-1A](https://pulsetechniques.com/): manufacturer description of the passive low boost/attenuation shelves, offset corners and combined low-end lift/dip, based on Audio Precision measurements.
- [Pultec EQP-1A operating instructions](https://www.sowter.co.uk/schematics/Pultec%20EQP%201A.pdf): original operating manual describing the passive equalizer, makeup amplifier and transformer I/O.
- [Sound On Sound — Pulse Techniques EQP-1A review](https://www.soundonsound.com/reviews/pulse-techniques-eqp-1a): published measurements note the attenuation corner sits roughly half an octave above the boost corner, supporting the current 1.414 ratio as an initial approximation.
- [Manley Enhanced Pultec EQP1-A response curves](https://static1.squarespace.com/static/582f5ce4e4fcb562b921bdc3/t/590cf29bc534a56c75914f20/1494020763255/Manley-PULTEC-EQ-Curves.pdf): published low/high response plots useful for curve-shape checks; Manley revisions are not a substitute for measuring the exact reference unit.
