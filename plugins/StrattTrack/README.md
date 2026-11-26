# StrattTrack 1.2.3

Stratt SSL-style stereo channel strip for Akai MPC / Force external effect hosting.

## Signal path
Stereo input → Console7 input stage → input trim → preamp → smoothed L/R polarity and channel exchange → equal-power dual input pan → M/S width and negative-correlation side safeguard → low-frequency Side reduction (Bass Mono) → original 12 dB/oct Butterworth HP/LP → optional serial Cleansweep HP/LP (independent HP/LP enable, 20 Hz–11 kHz / 20 Hz–22 kHz, 1st–6th-order Butterworth) → Pulse Techniques EQP-1A response-curve model → four-band parametric EQ → linked-stereo gate → linked-stereo compressor → controllable circuit drive and fixed valve-inspired transfer → smoothed stereo balance → tone-neutral Output master trim.

The dedicated PULTEC page exposes the EQP-1A controls: 20/30/60/100 Hz low boost and attenuation, 3/4/5/8/10/12/16 kHz high peak boost with sharp-to-broad bandwidth, and 5/10/20 kHz high shelving attenuation. The stage sits after HP/LP and before the four-band channel EQ. Control ranges follow Pulse Techniques' published specifications.

## DSP model and limits
The Pultec stage combines first-order low shelves, the measured higher attenuation turnover, a bandwidth-controlled RBJ peak section whose peak gain follows the measured sharp-versus-broad difference, a high-frequency attenuation shelf, and a broad low-frequency boost/attenuation interaction contour. The interaction term is a fitted digital approximation guided by Pulse Techniques' published EQP-1A specifications and Audio Precision response plots for the EQP-1A3. Pulse Techniques states that EQP-1A and EQP-1A3 use the same components and specifications. This is a curve-informed model, not a component-by-component passive-network simulation or a claim of bit-exact hardware identity: public curves are plotted sweeps rather than downloadable raw complex transfer data, so every intermediate knob position cannot be recovered exactly.

The four-band EQ uses RBJ cookbook bell biquads for LF, LM, HM and HF (LF/HF are not shelves); HP/LP are the shared second-order Butterworth sections used identically by StrattPultek and StrattTrack, with Q=0.7071, HP 20–1000 Hz, LP 1–22 kHz, a 10 ms logarithmic cutoff glide and 64-frame coefficient refresh. Input/output trim, preamp, circuit mix, input pan, polarity, Pultec controls, existing EQ controls, gate/compressor thresholds and ratios, makeup and circuit drive glide over a 10 ms control interval to reduce parameter-step artefacts. Dynamics values are smoothed in dB space before conversion to linear gain; the compressor lookup curve follows the smoothed ratio with a 1×–32768× overdrive range and less than 0.2% interpolation error. Gate and compressor use independent linked-stereo peak envelopes, 3 dB gate hysteresis and the hard-knee amplitude-domain compressor law. Circuit Mix at 0% bypasses the user-controlled parallel Drive/Circuit colour path; the fixed, low-level ADAA valve stage remains active independently. No allocation, locks or I/O occur in the audio callback.

## UI and visual identity
StrattTrack follows the StrattPultek visual standard: MPC amber kit and focus accent (#D89A48), charcoal background (#17191C), warm off-white text, muted grey labels, matching film-strip knobs, switches and faders, and the same 1280×628 panel background. The shared visual treatment is applied consistently to INPUT, FILTERS, PULTEC, EQ, DYNAMICS and OUTPUT pages. The INPUT page assigns Q-Link 1–11 to Preamp, Phase L, Phase R, Invert L-R, Pan L, Pan R, Width, Correlation, Bass Mono, Drive and Console Fader. Input trim remains available as the twelfth Input control. The OUTPUT page places stereo Out Pan before the final Output master trim; the redundant final Fader has been removed. Output remains parameter 0, preserving the MPC host's harmless master-gain nudge contract. The background artwork is supplied as `assets/stratt-bg.png`, with editable vector source `assets/stratt-bg.svg`. Only presentation is shared; StrattTrack retains its own controls, signal path and DSP.

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

A second, independent HP/LP bank follows the original channel HP/LP pair and precedes the Pultec and four-band EQ. Both filters have independent enable, logarithmically smoothed cutoff and 1st–6th-order Butterworth slope. Order changes use a 10 ms old/new filter crossfade to avoid clicks. The 1st-order setting is the gentle 6 dB/octave Butterworth baseline of bx_cleansweep Pro; higher orders use bilinear-transform Butterworth pole pairs (and a first-order section for odd orders), double-precision coefficient design, stable second-order sections and 64-frame coefficient refresh. The clean-room stage is disabled by default and runs its filter state while bypassed, with a 10 ms wet transition when enabled, preserving the existing signal path by default. This implements the Pro Butterworth order family, not proprietary Anti-Crush processing, Bessel/Chebyshev/Elliptic curves or spectrum-analysis UI.

The output path now uses Out Pan followed by the single Output master trim; the redundant Fader parameter and gain stage have been removed. LF and HF channel EQ bands now use bell/peaking responses with fixed Q=0.7071 rather than shelves.


## Order-transition refinement (1.2.3)

Changing the Cleansweep order now retains the previous HP/LP filter state and crossfades to the new Butterworth cascade over 10 ms. Newly activated sections are initialized safely, while the previous cascade continues processing only during the transition. Cutoff glide, bypass smoothing and the original first filter remain unchanged.


**Reference:** [Brainworx bx_cleansweep Pro manual](https://files.plugin-alliance.com/products/bx_cleansweep_pro/bx_cleansweep_pro_manual.pdf). This StrattTrack implementation deliberately reproduces the Butterworth 1st–6th-order HP/LP family only; it does not claim the Pro's other prototype families or Anti-Crush behavior.
