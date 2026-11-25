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

These are EQP-1A-inspired operating ranges and a digital curve approximation. The LF boost and attenuation and HF attenuation are implemented as first-order 6 dB/octave shelves, matching the documented passive-filter topology; the LF attenuation corner is offset higher than the boost corner and produces the characteristic combined lift/dip. The 1.414 LF corner ratio is a practical half-octave approximation. The HF attenuation selector is mapped to the documented mid-attenuation point (about -11.4 dB for a -20 dB maximum) by solving the discrete-time one-pole shelf response for each selected frequency and sample rate. This avoids the large error a fixed analog frequency ratio would create near Nyquist at 20 kHz / 44.1 kHz. The HF boost gain is reduced linearly with the broadness control from +20 dB at the narrow end to +13.5 dB at the broad end, following published Manley EQP-1A behaviour.

## Functional comparison with EQP-1A hardware and software models

| Function | EQP-1A / documented Manley revision | StrattPultek 1.2.0 | Assessment |
|---|---|---|---|
| LF boost and attenuation | Two interacting passive shelves; attenuation curve extends higher than boost | Two first-order shelves with a 1.414 corner ratio | Characteristic low-end lift/dip represented |
| LF selector | 20, 30, 60, 100 Hz | Same choices | Control coverage matches |
| HF boost | Peaking network; bandwidth changes width and peak gain | Biquad peak at the same choices; Q 1.60 to 0.45 and approximate +20 to +13.5 dB max scaling | Functional coverage and bandwidth-dependent gain represented |
| HF attenuation | High shelf, 5, 10, 20 kHz; selector is near the middle of the attenuation curve | First-order shelf; digital pole is solved to place the selector near -11.4 dB at a -20 dB maximum | Selector convention represented |
| Makeup / output stage | Passive loss compensated by a tube amplifier and transformers | Digital EQ followed by asymmetric polynomial ADAA saturation, DC blocking and output trim | Controllable harmonic colour; not a component-level tube model |
| Bypass / output routing | Hardware EQ bypass while amplifier remains in circuit (revision-dependent) | Host bypass plus wet/dry mix | Practical plugin routing |

## Findings from professional and community software emulations

The software comparison included Universal Audio Pultec Passive EQ, Waves PuigTec, Softube Tube-Tech PE 1C, Avid Pultec, Logic Vintage EQ and Analog Obsession Rare. Published tests show material differences between these products even when their controls are set to similar values:

- Input level changes the harmonic spectrum differently from one plugin to another; some models have a clear distortion threshold, some respond proportionally, and some produce little or no added distortion.
- Insertion gain and the behaviour of the signal path with the EQ section out are not consistent across products. Several emulations retain some colour or gain change when EQ is bypassed; others are closer to unity.
- A single static frequency-response curve is therefore not a sufficient description of the sound of a Pultec-style plugin.

StrattPultek applies this lesson by keeping the passive-EQ approximation separate from the colour stage. Drive controls how hard the signal enters the nonlinear stage, Grain controls its blend, and the neutral setting is transparent. This gives repeatable gain staging rather than an undocumented always-on distortion offset. The asymmetrical transfer introduces even-order content, while first-order ADAA reduces foldback without imposing an oversampling cost on the MPC ARM target.

## Circuit-modeling research and implementation decision

Barrera, Lizarraga-Seijas and Font (SMC 2024) describe an EQP-1A passive network derived from circuit schematics, LTspice simulation and Wave Digital Filters (WDF). Their paper reports that the optimized WDF implementation is suitable for real-time use and is faster than the compared Faust implementation. It also shows that oversampling is important to preserve high-frequency accuracy: the reported worst-case HF boost magnitude RMSE falls from 1.40 at 48 kHz to 0.22 at 192 kHz. The paper explicitly leaves the tube gain stage and input/output transformers outside its model.

That work is a useful technical reference and establishes that a more complete circuit-derived passive EQ is feasible without a physical unit as the only source of truth. StrattPultek 1.2.0 does not copy the GPL WDF implementation or claim identical WDF behaviour. It retains a compact shelf-plus-biquad model and an independent nonlinear colour stage to stay within the MPC/Force ARM real-time budget. The decision is a product trade-off: recognizable EQP-1A control interaction and musical response, predictable neutral settings, and a lightweight realtime path.

## CPU design

- One biquad (HF bell) and three first-order shelves per channel (LF boost, offset LF attenuation, HF attenuation); coefficients are updated once per process block.
- All eight EQ controls use a 15 ms block-size-compensated glide before coefficient recalculation, avoiding abrupt filter jumps while keeping the coefficient work out of the sample loop.
- One-pole 15 ms smoothing for output, grain, drive and mix.
- The colour stage uses first-order antiderivative antialiasing (ADAA). Its polynomial clip and squared-clip asymmetric component both have closed-form integrals, so the divided difference adds controlled even-order harmonics while reducing high-frequency foldback without per-sample log/exp/tanh or oversampling. A 3 Hz one-pole DC blocker removes the asymmetry's bias. Drive at 0% bypasses its contribution and Grain at 0% is fully clean.
- The ADAA and DC-blocker memories are per channel and reset with the filter state; the previous nonlinear input is not shared between stereo channels.
- Frequency selectors use logarithmic 15 ms smoothing; gain and bandwidth controls retain the 15 ms block-size-compensated glide. The three HF attenuation poles are solved once in prepare() for the active sample rate and cached; only the selected cached pole is smoothed in process().
- No heap allocation, locks, logging or I/O in the audio callback.

## Q-Link mapping

Normal: Q-Link 1 Low Boost (parameter 2), Q-Link 2 HF Boost (5), Q-Link 3 Grain (9).
SHIFT: Q-Link 1 Low Atten (3), Q-Link 2 HF Atten (8), Q-Link 3 Mix (11).
The first parameter remains Output trim, intentionally harmless under the MPC prepare-time parameter-0 nudge. Parameter order and IDs are stable.

## Validation performed

1. SDK build for native, ARM32 and ARM64; generated UI and tools/validate.py passed in GitHub Actions.
2. host_sim passed at 44.1, 48 and 96 kHz with 32-, 128- and 512-frame blocks.
3. Full validation rendered a reference WAV and packaged the installable MPC folder successfully.
4. Static and runtime validation do not substitute for timing on the physical Force hardware; the plugin is compiled for ARM32 and ARM64, but no live-device CPU benchmark is claimed.

## Public reference material

- [Pulse Techniques — EQP-1A](https://pulsetechniques.com/): manufacturer description of the passive low boost/attenuation shelves, offset corners and combined low-end lift/dip.
- [Pultec EQP-1A operating instructions](https://www.sowter.co.uk/schematics/Pultec%20EQP%201A.pdf): original operating manual describing the passive equalizer, makeup amplifier and transformer I/O.
- [Sound On Sound — Hidden Dangers of Vintage EQ](https://www.soundonsound.com/sound-advice/hidden-dangers-vintage-eq): comparative input-level distortion analysis of UAD, Waves, Softube, Logic and Analog Obsession.
- [Sweetwater — Pultec EQ Shootout](https://www.sweetwater.com/insync/pultec-shootout-with-sound-samples/): insertion gain and bypass-colour differences across hardware and software.
- [Universal Audio — Pultec Passive EQ Collection Manual](https://help.uaudio.com/hc/en-us/articles/7183176266260-Pultec-Passive-EQ-Collection-Manual): control groups, frequency selections and EQP-1A behaviour.
- [Barrera et al. — Modeling the Pultec EQP-1A with Wave Digital Filters (SMC 2024)](https://smcnetwork.org/smc2024/papers/SMC2024_paper_id132.pdf): circuit-derived passive-network modeling, oversampling and real-time implementation.
- [ABSounds — EQP-WDF-1A](https://github.com/ABSounds/EQP-WDF-1A): open-source WDF-based implementation used as a technical comparison; no source code is copied.
