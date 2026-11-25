# StrattPultek 1.0.0

**StrattPultek** is a stereo MPC insert effect inspired by the Pultec EQP-1A control topology.

## Included processing

- Selectable low-frequency boost (20, 30, 60, 100 Hz), combining a gentle shelf and resonant bell for a broader Pultec-style low-end shoulder.
- Simultaneous low attenuation control with a broad resonant dip approximation (the familiar low-end boost/attenuation interaction).
- Selectable high-frequency bell boost (3, 4, 5, 8, 10, 12, 16 kHz), with bandwidth control.
- High-frequency shelf attenuation (5, 10, 20 kHz).
- Low-cost asymmetric polynomial saturation with first-order antiderivative antialiasing (ADAA), adding controllable even-order harmonic content; independent Drive intensity and Grain blend controls, with a 3 Hz DC blocker. The clip curve is linear at low levels and has a smooth bounded knee; both controls at zero are genuinely transparent.
- Output trim and wet/dry mix; five factory starting programs.

## Modelling note

This is a **Pultec-inspired digital approximation**, not a claim of exact component-level hardware capture. The LF interaction is approximated with a broad bell dip, the HF controls with standard digital biquad sections, and the colour stage with a C1-continuous asymmetric polynomial soft clip, first-order ADAA and a slow DC blocker. The ADAA divided difference uses closed-form antiderivatives for both the clip and its asymmetric component to reduce aliasing without oversampling; the asymmetry adds even-order harmonics, and a 3 Hz DC blocker removes bias. EQ controls are smoothed across blocks to avoid abrupt coefficient changes when the user moves a control. Exact hardware matching requires measured reference curves and listening/level-matched validation against a specific serviced EQP-1A unit.

No Airwindows source code is copied into this plugin. The colour-stage design uses a generic low-cost waveshaping approach; no third-party Airwindows licence or attribution is required.

## CPU and realtime

Five stereo biquad sections are coefficient-updated once per block, with a 15 ms block-size-compensated parameter glide. Output, Grain, Drive and Mix use 15 ms one-pole smoothing. The colour stage uses a polynomial waveshaper and its closed-form antiderivative; it avoids per-sample transcendental functions and oversampling while reducing foldback distortion compared with direct sample-by-sample clipping. Frequency selections glide logarithmically. No allocation, locks, or I/O occur in the audio callback. ADAA is not alias-free, especially at extreme drive settings; verify the remaining foldback by measurement and listening.

## Build and test

```sh
python tools/build.py plugins/StrattPultek
.sdk-tools/host_sim plugins/StrattPultek/build/native/strattpultek.so test --sr 44100 --block 128 --wav plugins/StrattPultek/build/test.wav
python tools/validate.py plugins/StrattPultek
python tools/package.py plugins/StrattPultek
```

The UI uses the SDK's MPC amber widget kit and generated Q-Link skin; no external GUI framework or third-party assets are included.
