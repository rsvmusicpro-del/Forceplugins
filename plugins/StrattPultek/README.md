# StrattPultek 1.3.0

**StrattPultek** is a stereo MPC insert effect inspired by the Pultec EQP-1A control topology.

## Included processing

- Selectable first-order low-frequency boost and attenuation shelves (20, 30, 60, 100 Hz); the attenuation shelf uses a higher corner frequency to create the characteristic low-end lift/dip interaction.
- Selectable high-frequency bell boost (3, 4, 5, 8, 10, 12, 16 kHz), with bandwidth-dependent maximum gain (approximately +20 dB narrow to +13.5 dB broad).
- First-order high-frequency shelf attenuation (5, 10, 20 kHz); the selector is calibrated near the documented mid-attenuation point rather than the -3 dB pole, using a sample-rate-aware digital response solve.
- Low-cost asymmetric polynomial saturation with first-order antiderivative antialiasing (ADAA), adding controllable even-order harmonic content; independent Drive intensity, Grain blend and **Drive Mix** controls, with a 3 Hz DC blocker. Drive Mix is a dedicated post-EQ saturation dry/wet control and is smoothed over 15 ms.
- Airwindows PurestDrive-inspired sample-density weighting: the previous/current driven-sample sum dynamically reduces saturation blend around rapid zero-crossings/transients and favours sustained waveform body. This is a fresh implementation using the plugin's existing ADAA polynomial shaper, not copied Airwindows source.
- Drive gain compensation: the signal is boosted before the nonlinear stage and divided by the same gain afterward (`sat = ADAA(g·x) / g`), so Drive changes harmonic density rather than acting as a simple output-volume control. Drive Mix is a unity-sum parallel blend, avoiding the level bump that an equal-power crossfade can create with highly correlated dry/wet signals.
- Output trim and global EQ wet/dry mix; five factory starting programs.

## Software-emulation design basis

The design was reviewed against professional Pultec-style plug-ins (Universal Audio, Waves, Softube and Avid), as well as the free Analog Obsession Rare emulation and published software shootouts. Those comparisons show that the perceived identity of a Pultec plug-in is not only its static EQ curve: input-level-dependent harmonics, insertion gain, colour with EQ disengaged, and the way boost and bandwidth interact all materially affect the result. StrattPultek therefore keeps the passive-EQ controls separate from its controllable asymmetric colour stage, while retaining neutral Drive/Grain settings and a predictable host wet/dry path.

A published open-source EQP-1A study using Wave Digital Filters demonstrates that a schematic-derived passive network can be modeled in real time, with improved high-frequency accuracy when oversampled. That work informed the evaluation of the filter topology and the decision to keep the MPC build lightweight: the present implementation uses low-cost shelves and a resonant biquad rather than importing a third-party WDF implementation. This preserves the Force/MPC ARM target and its current CPU profile. It is a deliberate real-time design choice, not a claim that the two filter architectures are mathematically identical.

## Modelling note

This is a **Pultec-inspired digital emulation**, designed to reproduce the recognizable control interaction and musical response in a low-CPU MPC insert. It is not advertised as a component-by-component capture of one hardware revision. The LF interaction is modeled with two low shelves whose corner frequencies are offset. The HF boost uses a resonant biquad whose maximum gain falls with wider bandwidth; the HF attenuation uses a first-order shelf calibrated so its selected frequency is near the documented mid-attenuation point (about -11.4 dB for a -20 dB maximum). The colour stage uses a C1-continuous asymmetric polynomial soft clip, first-order ADAA and a slow DC blocker. The ADAA divided difference uses closed-form antiderivatives for both the clip and its asymmetric component to reduce aliasing without oversampling; the asymmetry adds even-order harmonics, and a 3 Hz DC blocker removes bias. EQ controls are smoothed across blocks to avoid abrupt coefficient changes when the user moves a control.

Airwindows informed the design in two specific ways: the density-dependent wet weighting described for PurestDrive and the parallel dry/wet concept of Drive/Density. Pultec behaviour remains grounded in the original control interactions and the UAD manual. No Airwindows source code or third-party WDF implementation is copied into this plugin; the DSP is implemented in this repository under its MIT licence.

## CPU and realtime

One stereo biquad and three stereo first-order shelves are updated once per block, with a 15 ms block-size-compensated parameter glide. Output, Grain, Drive, Drive Mix and the global Mix use 15 ms one-pole smoothing. The colour stage uses an asymmetric polynomial waveshaper and its closed-form antiderivatives; it avoids per-sample transcendental functions and oversampling while reducing foldback distortion compared with direct sample-by-sample clipping. The added density term is \`d[n] = clamp(2·|x[n] + x[n−1]|, 0, 1)\`, and the final colour blend is \`g = Grain × Drive × DriveMix × d[n]\`; all three controls are normalised to [0, 1]. Drive Mix defaults to 100%, preserving the former default saturation blend and existing parameter indices. At Drive = 0 or Grain = 0 or Drive Mix = 0, the saturation contribution is exactly zero. Frequency selections glide logarithmically. No allocation, locks, or I/O occur in the audio callback. ADAA is not alias-free, especially at extreme drive settings; this remains a trade-off for the MPC ARM CPU budget.

## Build and test

```sh
python tools/build.py plugins/StrattPultek
.sdk-tools/host_sim plugins/StrattPultek/build/native/strattpultek.so test --sr 44100 --block 128 --wav plugins/StrattPultek/build/test.wav
python tools/validate.py plugins/StrattPultek
python tools/package.py plugins/StrattPultek
```

The UI uses the SDK's MPC amber widget kit and generated Q-Link skin; no external GUI framework or third-party assets are included.

## References

- [Sound On Sound — Hidden Dangers of Vintage EQ](https://www.soundonsound.com/sound-advice/hidden-dangers-vintage-eq): level-dependent distortion differences across UAD, Waves, Softube, Logic and Analog Obsession.
- [Sweetwater — Pultec EQ Shootout](https://www.sweetwater.com/insync/pultec-shootout-with-sound-samples/): insertion gain and bypass-colour differences across hardware and software.
- [Universal Audio — Pultec Passive EQ Collection Manual](https://help.uaudio.com/hc/en-us/articles/7183176266260-Pultec-Passive-EQ-Collection-Manual): control groups, frequency selections and EQP-1A behaviour.
- [Airwindows — Drive](https://www.airwindows.com/drive/): parallel dry/wet saturation and tonal shaping concepts.
- [Airwindows — Density](https://www.airwindows.com/density/): smooth multi-stage saturation with independent dry blend.
- [Airwindows — PurestDrive](https://www.airwindows.com/purestdrive-vst/): sample-density-weighted saturation blend; the concept is reimplemented here without copying code.
- [Airwindows source repository](https://github.com/airwindows/airwindows): MIT-licensed reference algorithms; no source files imported.
- [Barrera et al. — Modeling the Pultec EQP-1A with Wave Digital Filters (SMC 2024)](https://smcnetwork.org/smc2024/papers/SMC2024_paper_id132.pdf): circuit-derived passive-network modeling, oversampling and real-time implementation.
- [ABSounds — EQP-WDF-1A](https://github.com/ABSounds/EQP-WDF-1A): open-source WDF-based implementation used as a technical comparison, not copied into this project.
