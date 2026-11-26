# StrattTrack 1.5.1

Stratt SSL-style stereo channel strip for Akai MPC / Force external effect hosting.

## 1.3.1 stability update\nThe permanent hybrid triode stage now fails closed on non-finite upstream samples, clears its per-channel ADAA/DC memory after a corrupted sample, and bounds its input to ±64 before evaluating the antiderivative; this prevents a single invalid or extreme sample from contaminating subsequent audio. The new independent numerical regression suite differentiates the hybrid antiderivative, compares ADAA divided differences against split Gauss–Legendre integration across both valve knees, and checks short-interval stability.\n\n## Signal path
Stereo input → Console7 input stage → preamp → smoothed L/R polarity and channel exchange → equal-power dual input pan → M/S width and negative-correlation side safeguard → 2nd-order Butterworth low-frequency Side high-pass (Bass Mono) → original 12 dB/oct Butterworth HP/LP → optional serial Cleansweep HP/LP (independent HP/LP enable, 20 Hz–11 kHz / 20 Hz–22 kHz, 1st–6th-order Butterworth) → Pulse Techniques EQP-1A response-curve model → fixed 16-band graphic EQ → linked-stereo gate → linked-stereo compressor → controllable circuit drive → Mono Compatibility → fixed valve-inspired transfer → smoothed stereo balance → tone-neutral Output master trim.

The dedicated PULTEC page exposes the EQP-1A controls: 20/30/60/100 Hz low boost and attenuation, 3/4/5/8/10/12/16 kHz high peak boost with sharp-to-broad bandwidth, and 5/10/20 kHz high shelving attenuation. The stage sits after HP/LP and before the 16-band graphic EQ. Control ranges follow Pulse Techniques' published specifications.

## DSP model and limits
The Pultec stage combines first-order low shelves, the measured higher attenuation turnover, a bandwidth-controlled RBJ peak section whose peak gain follows the measured sharp-versus-broad difference, a high-frequency attenuation shelf, and a broad low-frequency boost/attenuation interaction contour. The interaction term is a fitted digital approximation guided by Pulse Techniques' published EQP-1A specifications and Audio Precision response plots for the EQP-1A3. Pulse Techniques states that EQP-1A and EQP-1A3 use the same components and specifications. This is a curve-informed model, not a component-by-component passive-network simulation or a claim of bit-exact hardware identity: public curves are plotted sweeps rather than downloadable raw complex transfer data, so every intermediate knob position cannot be recovered exactly.

The 16-band graphic EQ uses stereo-linked RBJ peaking biquads at fixed logarithmic centres from 20 Hz to 20 kHz, with ±12 dB per band and gain-adaptive octave bandwidth: a smoothstep focus law moves from a broad 1.9-octave response toward each band's measured centre-spacing bandwidth as the slider approaches ±12 dB; the RBJ bilinear-transform bandwidth relation converts that octave width to a sample-rate- and centre-frequency-correct Q, including Nyquist headroom limiting. Boost/cut are symmetric, 0 dB is exactly unity, gain is smoothed over 10 ms and coefficients refresh every 64 frames; HP/LP are the shared second-order Butterworth sections used identically by StrattPultek and StrattTrack, with Q=0.7071, HP 20–1000 Hz, LP 1–22 kHz, a 10 ms logarithmic cutoff glide and 64-frame coefficient refresh. The 16 graphic-EQ gain smoothers and coefficients update at bounded 64-frame intervals, avoiding host-block-sized automation steps. Output master trim, Preamp, circuit mix, input pan, polarity, Pultec controls, graphic EQ gains, gate/compressor thresholds and ratios, makeup and circuit drive glide over a 10 ms control interval to reduce parameter-step artefacts. Dynamics values are smoothed in dB space before conversion to linear gain; the compressor lookup curve follows the smoothed ratio with a 1×–8,388,608× overdrive range and less than 0.2% interpolation error. Gate and compressor use independent linked-stereo peak envelopes, 3 dB gate hysteresis and the hard-knee amplitude-domain compressor law. Circuit Mix at 0% bypasses the user-controlled parallel Drive/Circuit colour path; the fixed, low-level ADAA valve stage remains active independently. No allocation, locks or I/O occur in the audio callback.

## UI and visual identity
StrattTrack follows the shared StrattPultek visual standard and repository MPC red UI kit, reflecting the native AKAI-inspired palette: charcoal background (#1E1D1E), red focus/accent (#D31145), white primary text and muted grey secondary labels. It uses the shared controls from `ui-kit/mpc-red` (knobs, faders, buttons, segments and toggles) and the matching 1280×628 panel background. The same visual treatment is applied consistently to INPUT, FILTERS, PULTEC, EQ, DYNAMICS and OUTPUT pages. EQ now presents sixteen bipolar faders in fixed-frequency order; the former LF/LM/HM/HF parametric controls are no longer shown or processed. The INPUT page assigns Q-Link 1–11 to Preamp, Phase L, Phase R, Invert L-R, Pan L, Pan R, Width, Correlation, Bass Mono, Drive and Console Fader. The former Input trim is removed from the UI and DSP; its VST parameter slot remains hidden and non-automatable to preserve all subsequent saved-project and automation indices. The OUTPUT page provides Mono Compatibility (Off/Auto/Full) and its adjustable Auto threshold; the DSP protection runs immediately before the valve stage, ahead of stereo Out Pan and the final Output master trim; the redundant final Fader has been removed. Output remains parameter 0, preserving the MPC host's harmless master-gain nudge contract. Original background artwork is retained in `assets/stratt-bg.png`, with editable vector source `assets/stratt-bg.svg`. Only presentation is shared; StrattTrack retains its own controls, signal path and DSP.

## Build and test
```sh
python tools/build.py plugins/StrattTrack
python tools/validate.py plugins/StrattTrack
python tools/package.py plugins/StrattTrack
```

The GitHub Actions workflow builds native, ARM32 and ARM64, validates the deployable folder, and exercises the host simulator at multiple sample rates and block sizes.


## Console emulation

The input Console stage is an attributed MIT-licensed adaptation of Airwindows Console7Channel, using its reference Fader law and channel transfer with polynomial ADAA for lower aliasing on MPC ARM hardware. It is not bit-identical to the original VST, and it does not claim to reproduce the separate Console buss summing/decode stage. See `DSP-NOTES.md`.


## Stereo field controls (1.1.5)

Width uses M=(L+R)/2, S=(L−R)/2 and reconstructs L=M+wS, R=M−wS: 100% is transparent, 0% is mono, and 200% doubles existing Side energy without manufacturing stereo from mono. Correlation is an adaptive safeguard, not a correlation meter or automatic phase-alignment claim: it estimates normalized L/R correlation over a 50 ms one-pole window and attenuates Side only in proportion to negative correlation. At 0% it is transparent. Bass Mono removes the low-passed Side component using a smoothed one-pole cutoff (20–400 Hz), retaining the Mid/mono bass. Invert L-R smoothly exchanges channel positions; Phase L/R remain independent polarity switches. These tools reduce common compatibility risks but cannot guarantee mono compatibility for every source; audition the mono sum.


## Mono-invariant channel exchange (1.1.9)

The M/S Side inversion coefficient is calculated once per host block rather than evaluating cosine per sample, keeping the audio loop free of per-sample trigonometric coefficient work.

Invert L-R now performs its transition in the M/S domain: Mid remains exactly unchanged while Side is multiplied by cos(πt), t∈[0,1]. At the endpoints this is transparent or a true L/R exchange; during the transition a dual-mono signal retains unity gain instead of receiving the +3 dB midpoint bump caused by an equal-power crossfade.

## Click-safe polarity switching (1.1.8)

The per-channel zero-crossing detector now explicitly seeds its previous-sample history before accepting a crossing; a polarity toggle immediately after reset can no longer mistake the initial zero-valued history for a real crossing and invert at an arbitrary non-zero sample. The existing bounded 20 ms timeout and 1 ms fallback remain unchanged.

Phase L/R are discrete polarity inversions, not continuous phase rotation. A switch commits at the next channel-specific zero crossing; DC or near-DC content uses a bounded 20 ms wait followed by a 1 ms one-pole transition. This avoids the long amplitude dip caused by linearly smoothing a polarity multiplier from +1 to -1.


## Serial Cleansweep stage (1.2.2)

A second, independent HP/LP bank follows the original channel HP/LP pair and precedes the Pultec and 16-band graphic EQ. Both filters have independent enable, logarithmically smoothed cutoff and 1st–6th-order Butterworth slope. Order changes use a 10 ms old/new filter crossfade to avoid clicks. The 1st-order setting is the gentle 6 dB/octave Butterworth baseline of bx_cleansweep Pro; higher orders use bilinear-transform Butterworth pole pairs (and a first-order section for odd orders), double-precision coefficient design, stable second-order sections and 64-frame coefficient refresh. The clean-room stage is disabled by default and runs its filter state while bypassed, with a 10 ms wet transition when enabled, preserving the existing signal path by default. This implements the Pro Butterworth order family, not proprietary Anti-Crush processing, Bessel/Chebyshev/Elliptic curves or spectrum-analysis UI.

The output path now uses Out Pan followed by the single Output master trim; the redundant Fader parameter and gain stage have been removed. LF and HF channel EQ bands now use bell/peaking responses with fixed Q=0.7071 rather than shelves.


## Order-transition refinement (1.2.3)

Changing the Cleansweep order now retains the previous HP/LP filter state and crossfades to the new Butterworth cascade over 10 ms. Newly activated sections are initialized safely, while the previous cascade continues processing only during the transition. Cutoff glide, bypass smoothing and the original first filter remain unchanged.


**Reference:** [Brainworx bx_cleansweep Pro manual](https://files.plugin-alliance.com/products/bx_cleansweep_pro/bx_cleansweep_pro_manual.pdf). This StrattTrack implementation deliberately reproduces the Butterworth 1st–6th-order HP/LP family only; it does not claim the Pro's other prototype families or Anti-Crush behavior.


## Input gain consolidation (1.2.4)

The separate −24 to +24 dB Input trim and its gain stage have been removed. Preamp is now the first INPUT-page control and the only active user input-gain stage after Console7. The former `input` parameter ID remains as a zero-range, non-automatable compatibility slot at index 1; it is not displayed, mapped to an MPC Sample knob, or read by the DSP. All 50 parameter positions, factory-program values and later parameter IDs remain unchanged.


## Realtime polarity fallback (1.2.5)

The 1 ms fallback coefficient and 20 ms zero-crossing timeout are now calculated once in `prepare()` from the validated sample rate, rather than recalculating an exponential and timeout conversion inside the per-sample polarity-switch path. The click-safe zero-crossing behavior, timeout duration, fallback trajectory, parameter identity and audio transfer remain unchanged; this removes avoidable transcendental work from the real-time callback.


## Hybrid valve voicing (1.2.7)

The permanent parameter-free valve stage blends normalized ECC83/12AX7-like high-mu and 6SN7-like medium-mu triode curves (72/28), with asymmetric C2 soft knees, exact hybrid antiderivative ADAA, and subtle 18 kHz nonlinear-residual softening. Its chain position, permanent operation and independence from Drive/Circuit Mix are unchanged; no parameter IDs or Q-Link mappings were added. This is an original curve-informed voicing model, not a component-level simulation or measured vintage-valve capture; see `DSP-NOTES.md` for equations, electrical anchors, references and limitations.


## Correlation confidence floor (1.2.8)

The 50 ms stereo correlation estimator now ignores its negative-correlation
safeguard while either channel's smoothed mean-square level is below −90 dBFS
(`1e-9` mean square). This prevents silence and quantization residue from
driving an unnecessary Side reduction. The M/S equations, mono fold-down,
width range, pan law and all existing parameter IDs remain unchanged.


## Mono Compatibility (1.3.0)

The OUTPUT page adds a three-mode mono-protection control. Off bypasses this additional protection. Auto monitors the 50 ms normalized L/R correlation estimate after the other channel processing and progressively reduces Side only when correlation reaches or falls below the adjustable negative threshold (0–100%, default 50% = rho −0.5); it reaches zero Side at rho −1. Full sets Side to zero immediately before the valve stage, producing a dual-mono signal at that point. The protection is smoothed over 10 ms and never alters Mid. Auto reduces correlation-related mono risk but cannot guarantee compatibility for every downstream nonlinear or time-varying process; Out Pan remains a separate downstream balance control. The existing Correlation safeguard and Bass Mono remain independent controls. The two new parameters are appended after all 50 existing parameter slots; prior IDs, presets and MPC Sample mappings retain their indices.

## Stereo image refinements (1.2.9)

Phase L/R remains a zero-crossing polarity inversion with a bounded DC fallback; Invert L-R smoothly reverses Side while preserving Mid. Width remains mono-sum invariant and equal-power Pan retains its constant-power law. Pan, Width and M/S inversion automation now refreshes every 64 frames with elapsed-frame smoothing rather than stepping once per host callback. Bass Mono now uses a dedicated second-order Butterworth high-pass on Side (12 dB/octave, 20–400 Hz), keeping the centre signal unchanged; the design is informed by Maselec MTC-1X elliptical filtering and Drawmer 1976 low-band width practice, without claiming exact hardware emulation.


## 16-band graphic EQ (1.5.1)

The former four-band parametric EQ audio path is replaced by sixteen stereo-linked RBJ peaking filters at fixed logarithmic centres: 20, 31.5, 50, 80, 125, 200, 315, 500, 800, 1250, 2000, 3150, 5000, 8000, 12500 and 20000 Hz. Each slider spans −12 to +12 dB. The gain-dependent proportional-Q curve widens subtle moves (Q≈0.707) and focuses stronger cuts/boosts progressively (Q=2.146 at ±12 dB), with symmetric boost/cut and exact unity at 0 dB. Gain is smoothed over 10 ms and coefficients refresh every 64 frames. This is a minimum-phase IIR cascade with no added latency and no audio-thread allocation. The former ten EQ parameters remain as inert compatibility slots at their original indices; the sixteen new gain parameters are appended, preserving existing project parameter ordering. See `GRAPHIC-EQ-RESEARCH.md` for hardware/software comparisons and limitations.
