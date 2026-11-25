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
| Grain / Drive | 0–100% each | Rational tanh waveshaper, unity small-signal slope |
| Output / Mix | -12…+12 dB / 0–100% | Output trim and parallel blend |

These are EQP-1A-inspired operating ranges and a digital curve approximation. They are not asserted as measured plots of a particular hardware serial number. Different Pultec revisions and component tolerances differ; exact reproduction requires a selected reference unit and measured sweeps.

## CPU design

- Four biquad sections per channel; coefficients computed once per process block.
- One-pole smoothing for output, grain, drive and mix.
- Rational waveshaper uses only multiplies and divides in the sample loop.
- No oversampling in this low-CPU baseline. High drive can alias; a later 2x/4x option should be added only if hardware measurements justify the CPU cost.
- No heap allocation, locks, logging or I/O in the audio callback.

## Q-Link mapping

Normal: Q-Link 1 Low Boost (parameter 2), Q-Link 2 HF Boost (5), Q-Link 3 Grain (9).
SHIFT: Q-Link 1 Low Atten (3), Q-Link 2 HF Atten (8), Q-Link 3 Mix (11).
The first parameter remains Output trim, intentionally harmless under the MPC prepare-time parameter-0 nudge.

## Required bench checks

1. SDK build for native, ARM32 and ARM64; generated UI and tools/validate.py must pass.
2. host_sim test at 44.1 kHz with 32-, 128- and 512-frame blocks, random parameter sweep, state round-trip, in-place processing and silence pass-through.
3. Frequency sweeps at all LF/HF selections; verify finite output and expected shelf/peak/dip directions and no instability.
4. Measure CPU on Force hardware; host_sim desktop timing is not a substitute for ARM timing.
5. Level-matched listening against a serviced EQP-1A reference before describing the response as hardware-matched.