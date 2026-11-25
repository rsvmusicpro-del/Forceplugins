# StrattPultek 1.0.0

**StrattPultek** is a stereo MPC insert effect inspired by the Pultec EQP-1A control topology.

## Included processing

- Selectable low-frequency shelf boost (20, 30, 60, 100 Hz).
- Simultaneous low attenuation control with a broad resonant dip approximation (the familiar low-end boost/attenuation interaction).
- Selectable high-frequency bell boost (3, 4, 5, 8, 10, 12, 16 kHz), with bandwidth control.
- High-frequency shelf attenuation (5, 10, 20 kHz).
- Low-cost, rational soft saturation for adjustable harmonic grain/warmth.
- Output trim and wet/dry mix; five factory starting programs.

## Modelling note

This is a **Pultec-inspired digital approximation**, not a claim of exact component-level hardware capture. The LF interaction is approximated with a broad bell dip, the HF controls with standard digital biquad sections, and the colour stage with a low-cost rational tanh approximation. Exact hardware matching requires measured reference curves and listening/level-matched validation against a specific serviced EQP-1A unit.

No Airwindows source code is copied into this plugin. The colour-stage design uses a generic low-cost waveshaping approach; no third-party Airwindows licence or attribution is required.

## CPU and realtime

Four stereo biquad sections are coefficient-updated once per block. The colour stage uses multiplies/divides per sample and no oversampling, allocation, locks, I/O, or transcendental functions in the audio loop. Oversampling is intentionally omitted for the first test build to keep ARM CPU use low; extreme drive settings can generate some aliasing.

## Build and test

```sh
python tools/build.py plugins/StrattPultek
.sdk-tools/host_sim plugins/StrattPultek/build/native/strattpultek.so test --wav plugins/StrattPultek/build/test.wav
python tools/validate.py plugins/StrattPultek
python tools/package.py plugins/StrattPultek
```

The UI uses the SDK's MPC amber widget kit and generated Q-Link skin; no external GUI framework or third-party assets are included.
