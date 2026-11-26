# StrattTrack DSP notes

## Shared HP/LP filter standard (1.1.2)

StrattTrack and StrattPultek use the same TwoPoleButterworth implementation from framework/dsp/two_pole_filter.h. Both use second-order Butterworth high-pass and low-pass sections (Q=1/sqrt(2)), RBJ bilinear-transform coefficients designed in double precision, a 10 ms logarithmic cutoff glide and 64-frame coefficient refresh. HP range is 20–1000 Hz and LP range is 1000–22000 Hz; coefficients are bounded to 0.45 × sample rate. Invalid input/output resets only the affected filter state. The sections are cascaded HP then LP before the Pultec/channel EQ stages.

## Signal path and algorithms
- Input gain consolidation: Preamp is the only active user input-gain stage. The former `input` ID remains as a hidden, non-automatable zero-range compatibility slot at index 1; it is not read by the DSP.
- Preamp: linear dB gain with sample-rate-invariant smoothing and finite input guards.
- Polarity and pan: 10 ms block-compensated smoothing avoids abrupt polarity/pan discontinuities; equal-power input pan maintains constant summed source power.
- HP/LP: shared second-order Butterworth sections (Q=1/sqrt(2)), bilinear-transform coefficients, 10 ms logarithmic smoothing and 64-frame coefficient refresh.
- Pulse Techniques EQP-1A stage (after filters, before four-band EQ):
  - LF boost and attenuation use first-order low shelves with selectable 20/30/60/100 Hz turnovers.
  - The attenuation turnover is offset by sqrt(2), matching the approximately half-octave higher corner visible in EQP-1A3 response sweeps.
  - A broad LF boost/attenuation interaction bell approximates the measured combined lift-and-dip contour; its depth is bounded and saturating, centred at 8× the selected LF frequency.
  - HF boost uses an RBJ peaking section at 3/4/5/8/10/12/16 kHz; Q maps from 1.60 (sharp) to 0.45 (broad); the maximum peak gain is reduced by 9 dB from sharp to broad, matching the published measurement comparison.
  - HF attenuation is a first-order high shelf at 5/10/20 kHz.
  - Controls are smoothed in log-frequency or linear gain space; coefficients are calculated once per block; filter memory is per channel.
- Gate threshold/range, compressor threshold/ratio/makeup and circuit drive use a 10 ms control-rate smoother; dynamics thresholds and makeup glide in dB space before conversion to linear amplitude. The compressor curve LUT is rebuilt from the smoothed ratio, avoiding abrupt gain-computer changes without adding per-sample transcendental math.
- Four-band EQ: all four bands (LF, LM, HM and HF) are RBJ peaking/bell biquads; LF and HF are deliberately not shelves. LF/HF use fixed Q=0.7071, LM/HM use their user-selected Q (0.4–4.0), and every band is exactly unity at 0 dB. Frequency, gain and Q smoothers advance on a 64-frame cadence, and all four biquad coefficient sets are refreshed at that same cadence using the elapsed-frame one-pole coefficient `1-exp(-N/(0.010·fs))`; this bounds automation steps independently of the host callback size.
- Dynamics: linked-stereo peak sensing, independent exponential gate/compressor detectors, 3 dB gate hysteresis, 2 ms gate gain slew, hard-knee compressor amplitude law and post-reduction makeup.
- Circuit: bounded rational Drive/Circuit colour is parallel-blended and fully bypassed at 0%; the separate fixed ADAA valve-inspired stage and 3 Hz DC blocker remain active independently after the blend.
- Output: tone-neutral master trim and smoothed stereo balance.

## Verification equations
CI independently checks Butterworth corner magnitude, neutral RBJ peak response, equal-power pan identity, hard-knee compressor ratio, one-pole time constant, DC-blocker zero at DC, Pultec shelf endpoints, neutral Pultec transparency, bounded LF interaction, parameter identity and stage order.

## References
- Pulse Techniques, EQP-1A product specifications: https://pulsetechniques.com/products/tube-equalizers/eqp-1a/
- Pulse Techniques, EQP-1A3 history and bass boost/attenuation interaction: https://pulsetechniques.com/
- Sound On Sound, Pulse Techniques EQP-1A review, including Audio Precision frequency-response sweeps for bass boost/cut, HF boost and HF attenuation: https://www.soundonsound.com/reviews/pulse-techniques-eqp-1a
- Robert Bristow-Johnson, Audio EQ Cookbook: https://www.w3.org/TR/audio-eq-cookbook/
- Giannoulis, Massberg and Reiss, Digital Dynamic Range Compressor Design—A Tutorial and Analysis, JAES 2012: https://aes.org/publications/elibrary/elibrary-page/?id=16354

No third-party DSP source code is copied. This original digital response model is curve-informed, not an exact component-level reconstruction; published plotted sweeps do not provide raw complex transfer data for every potentiometer position or hardware unit.

## Continuous invisible valve stage

StrattTrack now uses the same fixed triode-inspired valve stage as StrattPultek 1.9.9:
a C2-continuous soft knee, an asymmetric even-harmonic term, first-order ADAA using
closed-form antiderivatives, a stable short-interval expansion/quadrature fallback,
and a 3 Hz DC blocker. It is permanently active, has no parameter or UI mapping,
and is independent of the user-controlled Drive/Circuit Mix. It follows the
channel-strip tone and dynamics stages and precedes the final tone-neutral Output master gain.

The intended result is subtle harmonic density, gentle peak rounding and a
slightly richer midrange without an audible level jump or uncontrolled DC offset.
This is a deterministic triode-inspired nonlinear model, not a component-level
simulation of a specified valve, transformer or measured circuit. ADAA reduces
waveshaper aliasing but is not a claim of zero aliasing; host-simulator renders
and hardware listening remain necessary validation.



## Host-state and sample-rate hardening (1.1.1)

The reset/prepare path now validates every restored continuous control before seeding its smoother and clamps all choice indices before table lookup. This matters because a non-finite value stored by a host or restored project can otherwise poison a one-pole smoother: once its internal state becomes NaN, later valid automation cannot recover it. The shared smoother also rejects non-finite state, targets and coefficients defensively. The active sample rate is constrained to the supported 8–384 kHz range, with a 44.1 kHz fallback for malformed host input, before the Console7 filter and valve DC-blocker coefficients are prepared. Parameter IDs, parameter order, preset values and signal-path topology are unchanged.

## Airwindows Console7 input console stage

The first stage of the signal path is a Console7Channel-derived input console, with the Airwindows reference control retained as a single **Fader** control (0.000–1.000; default 0.772). It reproduces the reference channel topology: 20 kHz low-pass (Q=1.618), the original nonlinear gain law (Fader × 1.2720196495, gain cubed before the transfer and divided by gain afterwards), and the original 80:20 Spiral/Density transfer. The control is smoothed over 1.5 ms and is processed independently per stereo channel before any plugin-specific input/EQ/filter stage.

For the MPC ARM real-time budget, the trigonometric transfer is replaced by its degree-21 convergent polynomial (maximum static-curve error below 1.5e-7 over the clipped ±1.097 domain) and first-order antiderivative antialiasing. This preserves the Airwindows curve to numerical precision while reducing alias foldback; it is intentionally not bit-identical to the original non-antialiased VST. The original Airwindows Console7Channel source is MIT licensed; this implementation is an attributed adaptation, not a binary/source copy of the VST wrapper. The channel stage is used here as a single-plugin insert; the full multitrack Console system's separate buss decode/summing stage is not represented.

### What engineers seek from a console path

A console pass is generally used for coherent summing, subtle harmonic/intermodulation density, gentle peak rounding, a little dynamic softening and a more cohesive spatial impression while retaining source identity, transient intelligibility and tonal balance. The selected Console7 channel curve contributes this controlled nonlinear density; no noise, arbitrary EQ tilt or aggressive compression is added. The pre-existing valve stage remains a separate output coloration stage with its own analytic ADAA transfer and DC blocker, so the input-console emulation does not replace or bypass it.


## Per-sample control smoothing correction (1.1.3)

The 10 ms control one-pole coefficient is now calculated for one sample, because every `Smooth::next()` call occurs inside the sample loop. The previous coefficient was calculated for the entire host block and then reused for every sample, making parameter transitions block-size dependent and up to the block length faster than intended. The corrected coefficient is `alpha = 1 - exp(-1 / (0.010 * fs))`; after 10 ms the smoother reaches approximately 63.2% of a step at 44.1, 48 and 96 kHz, independently of whether the host processes 32, 128 or 512 frames per call. This affects control trajectories only; steady-state EQ, dynamics, gain curves, parameter identities, presets and signal-path topology are unchanged.


## Input stereo field controls (1.1.6)

- Invert L-R: smooth M/S-domain exchange; Mid is invariant and Side is multiplied by cos(πt), yielding exact normal/swapped endpoints without a mono-level bump. It is separate from L/R polarity inversion.
- Width: M=(L+R)/2, S=(L−R)/2, decode L=M+wS, R=M−wS. 100% is unity; 0% collapses to mono; values above 100% amplify only existing Side content.
- Correlation: 50 ms exponentially weighted estimates of E[L²], E[R²] and E[LR], normalized to rho=E[LR]/sqrt(E[L²]E[R²]). The 0–100% amount attenuates Side proportionally to max(0,−rho). This is a negative-correlation safeguard, not phase alignment, delay estimation, a display meter, or a guarantee of compatibility.
- Bass Mono: one-pole low-pass extraction of Side below a 20–400 Hz cutoff, then removal of that low band from Side. Mid remains intact; cutoff is smoothed and filter state is per instance.
- Controls are smoothed; no allocations, locks, logging or I/O are introduced in process().

Engineering basis: mid/side and mono-sum behaviour are described by iZotope's M/S processing guide and PatchLab's stereo/mono notes; negative correlation and low-frequency centering are discussed by sonible's mono-compatibility guide and Sweetwater's phase-meter reference. These controls do not replace auditioning the actual mono fold-down.


## Click-safe polarity switching (1.1.6)

Phase L/R are polarity inversions (0°/180°), not continuous phase rotation controls. A parameter change now commits at the first per-channel zero crossing after valid previous-sample history has been seeded, preserving full amplitude and avoiding the artificial 10 ms cancellation trough caused by linearly smoothing +1 to -1. The initial reset value is never treated as a real crossing. For DC or near-DC material without a crossing, a 20 ms bounded wait switches to a 1 ms one-pole fallback. The previous sample, requested polarity, wait counter and fallback state are independent per channel and reset deterministically. No allocation, locks, trigonometric operations or I/O are added to the sample path.


## Sample-rate-invariant gain smoothing (1.2.0)

Output master trim, Preamp and parallel circuit mix now use an explicit one-pole coefficient derived from the actual sample rate, giving each control the same 10 ms time constant at 44.1, 48 and 96 kHz (and other supported rates). The smoother API no longer has a hard-coded per-sample default, so future call sites must choose a rate-correct coefficient explicitly. Console Fader retains its intentional 1.5 ms response; block-rate controls retain elapsed-block 10 ms smoothing.

The StrattTrack contract test verifies the sample-rate-derived one-sample coefficient against the 10 ms response at 44.1, 48 and 96 kHz and enforces explicit coefficients at all gain-control smoother call sites.


## Compressor transfer accuracy and headroom (1.2.1)

The compressor's static gain-computer lookup now uses a 1,024-interval fourth-power overdrive grid spanning 1× to 8,388,608× threshold (approximately +138.5 dB). The denser grid near unity resolves the hard-knee transition accurately, while its extended upper range prevents the compressor ratio from flattening prematurely on hot signals. The sample loop maps the normalized detector overdrive with two square roots and table interpolation; it performs no per-sample logarithm or power operation. Independent numerical checks cover ratios from 1:1 through 20:1 across the full table range, with maximum relative interpolation error below 0.2%. The plugin binary and manifest version are incremented to 1.2.1; parameter identity, order, presets, state compatibility and signal-path order are unchanged.


## Independent serial Cleansweep filter bank (1.2.2)

The existing HP/LP Butterworth filters are unchanged. A second, independently enabled HP/LP bank is inserted immediately after them and before Pultec/channel EQ. Each side has its own 20–11 kHz / 20–22 kHz cutoff, 1–6 order selector and 10 ms smoothed enable transition. Order N uses the standard Butterworth pole decomposition: Qk = 1 / (2 sin((2k+1) pi/(2N))). RBJ bilinear-transform sections are designed in double precision at 64-frame control intervals, with the real-pole section prewarped so the analog corner maps to the digital -3 dB point. This produces 6, 12, 18, 24, 30 or 36 dB/octave Butterworth slopes. The stage is off by default and its state continues tracking the signal while bypassed, avoiding stale-state transients on activation.

The implementation provides the bx_cleansweep Pro Butterworth order family as a compact MPC-ARM-friendly clean-room design; it does not claim the proprietary Anti-Crush algorithm or reproduce the Pro-only Bessel, Chebyshev I/II, elliptic or resonant-Butterworth modes. The original HP/LP pair, EQ/dynamics stages, Console7 and MPC Sample Q-Link assignments remain otherwise unchanged. LF and HF parametric EQ bands are now bell sections (fixed Q=0.7071), matching the LM/HM bell topology. Output master gain is the only final level control; Out Pan is placed before it and redundant Fader is removed.


## Cleansweep order-transition refinement (1.2.3)

When an HP or LP order changes, the old per-channel filter instance is retained as a shadow state and crossfaded against the new cascade over the existing 10 ms sample-rate-aware smoother. Newly activated pole sections are reset before use; cutoff-only changes do not reset state. The shadow filter is evaluated only during the transition, keeping steady-state CPU cost unchanged.


## Reference and scope

The [official Brainworx bx_cleansweep Pro manual](https://files.plugin-alliance.com/products/bx_cleansweep_pro/bx_cleansweep_pro_manual.pdf) documents Butterworth, Bessel, Chebyshev I/II, Elliptic and Resonant Butterworth modes, each with order and type-dependent parameter behavior. StrattTrack 1.2.3 implements the independent Butterworth 1st–6th-order family with cutoff and enable per side; the other four/five prototype variants, type-dependent ripple/selectivity/resonance controls, analyzer and proprietary Anti-Crush are explicitly outside this implementation's scope.


## Input gain consolidation (1.2.4)

The separate −24 to +24 dB Input trim and its gain multiplication have been removed. Preamp is the only active user input-gain stage after Console7. The former `input` parameter ID remains as a hidden, zero-range, non-automatable compatibility slot at index 1, preserving the 50-parameter VST order and factory-program layout without an active UI control or audio-path read. The compressor's 8,388,608× lookup ceiling is retained as conservative headroom for combined EQ peaks; its 1,024-interval accuracy is independently verified across ratios 1:1–20:1 with less than 0.2% relative interpolation error.


## Documentation/implementation contract correction

The four-band channel EQ is implemented as four peaking (bell) biquads. Older wording that described LF/HF as shelves was incorrect; the README, C++ coefficient setup and contract tests all specify peaking responses. This correction records the actual shipped transfer function and prevents a future maintainer from changing the LF/HF topology unintentionally.


## StrattTrack 1.2.6 — parametric EQ automation refinement

The four channel-EQ bands now advance their 10 ms parameter smoothers and recalculate RBJ coefficients every 64 frames, matching the existing HP/LP refresh cadence. Previously, LF/LM/HM/HF coefficients were updated only once per host callback; with a 512-frame callback this produced comparatively coarse control steps during fast automation. The revised cadence reduces zipper-like modulation while preserving the static response, neutral gain, parameter IDs/order, factory programs, signal-path position and MPC Sample mapping. The one-pole coefficient for each update uses the actual chunk length N: `α = 1 − exp(−N/(τ·fs))`, with τ = 10 ms, so partial final chunks and variable host block sizes retain the intended time constant.


## StrattTrack 1.2.7 — hybrid triode voicing

The fixed valve stage stays permanently active in its original position after Drive/Circuit blend and before output balance/master trim; no parameter or UI mapping is added. The original single asymmetric waveshaper is refined into a normalized two-branch model: 72% ECC83/12AX7-like high-mu response and 28% 6SN7-like medium-mu response. Separate asymmetry constants (0.032, 0.060) and a 1.18 knee scale provide a gently earlier second branch; the combined small-signal slope remains unity. These are curve-informed electrical-family references, not measured Mullard/Telefunken/RCA unit captures or a full voltage-domain netlist.

For the base soft knee F(x)=sgn(x)[1+t−t³+0.5t⁴], t=|x|−1, with linear and bounded outer branches, T(x)=F(x)+ηF(x)². The hybrid transfer is H(x)=(1−w)T₁(x)+wT₂(kx)/k (w=0.28, k=1.18). Its exact antiderivative is A(x)=(1−w)[G(x)+η₁J(x)]+w[G(kx)/k²+η₂J(kx)/k²], where G'=F and J'=F². First-order ADAA evaluates [A(xₙ)−A(xₙ₋₁)]/(xₙ−xₙ₋₁); very short intervals use an exact quadratic closed-form path when both samples remain inside the two linear branches, and a constant fast path on same-polarity plateaus; remaining short intervals use five-point Gauss–Legendre quadrature split at both branches' knees and zero, avoiding cancellation while remaining exact for each degree-eight polynomial segment.

Only the nonlinear residual is softened: an 18 kHz one-pole memory filters 72% of the residual and 28% remains direct. The linear small-signal path remains unity and the per-channel DC blocker remains at 3 Hz. The filter coefficient is prepared outside the sample loop; all state is per channel. No allocation, lock, I/O or per-sample transcendental operation is introduced. This is an original voicing approximation; subjective claims require listening tests on representative material and physical MPC hardware.

### Valve references and electrical anchors

Sound descriptions are circuit/application tendencies, not guarantees: bias, load line, feedback, transformer, supply and operating class dominate the measured result.

- **Mullard ECC83 / 12AX7, high-mu small-signal dual triode:** μ≈100, rₚ≈62.5 kΩ, gₘ≈1.6 mA/V; representative point Vₐ=250 V, Vg=−2 V, Iₐ≈1.2 mA; heater 6.3 V/300 mA or 12.6 V/150 mA; maximum plate voltage 300 V, plate dissipation ≈1 W/section. Classic high-gain preamp type; selected for articulate gain-stage asymmetry.
- **Telefunken ECC83 / 12AX7:** same nominal type parameters (μ≈100, gₘ≈1.6 mA/V, rₚ≈62.5 kΩ); cited example: 250 V plate, −2 V grid, 1.2 mA plate current, 300 V maximum plate voltage and 1 W plate dissipation. Brand reputation for consistency/low noise does not imply a different nominal transfer law.
- **RCA 6SN7GT, medium-mu twin triode:** μ≈20 (gₘ≈2.6 mA/V × rₚ≈7.7 kΩ), gₘ≈2.6 mA/V, rₚ≈7.7 kΩ; typical point about 250 V plate, −8 V grid and 9 mA; heater 6.3 V/0.6 A. Lower amplification factor and plate resistance suit moderate voltage-gain/driver duties.
- **Mullard ECC82 / 12AU7, medium-mu dual triode:** μ≈13, gₘ≈1.7–3.0 mA/V; at 250 V plate and −8.5 V grid, Iₐ≈8.5–11.5 mA; heater 6.3 V/300–350 mA or 12.6 V/150–175 mA; maximum plate voltage 300 V, dissipation 2.75 W/section. Useful lower-gain, higher-current buffer/driver reference.
- **Mullard EL34, audio output pentode:** heater 6.3 V/1.5 A, 25 W anode dissipation; at 250 V plate/screen and −12.2 V grid, Iₐ≈100 mA, gₘ≈11 mA/V, rₚ≈15 kΩ. Mullard push-pull distributed-load data gives 20 W at 0.35% THD or 34 W at 2.5% at its stated 430 V conditions; a power-stage rather than preamp reference.
- **RCA 6L6GC, beam-power output valve:** heater 6.3 V/0.9 A; typical Class-A₁ point 250 V plate/screen, −14 V grid, Iₐ≈72 mA, gₘ≈6 mA/V, rₚ≈22.5 kΩ; maximum plate dissipation 30 W. Contrasting output-stage power/overdrive envelope.

ECC83 and 6SN7 families are the two selected small-signal curve anchors; ECC82, EL34 and 6L6GC are documented alternatives, not claimed as simulated by this stage.

References:
- Dempwolf & Zölzer, “A Physically-Motivated Triode Model for Circuit Simulations,” DAFx-11: https://dafx.de/paper-archive/2011/Papers/76_e.pdf
- N. Koren, “Improved vacuum-tube models for SPICE simulations,” Glass Audio 8(5), 1996.
- Mullard ECC83 / 12AX7 characteristics: https://www.worldradiohistory.com/Archive-Electronics/60s/60/Electronics-1960-10-21.pdf
- Telefunken ECC83-TK specifications: https://www.bhphotovideo.com/c/product/1502640-REG/telefunken_matched_quad_of_ecc83_tk.html/specs
- RCA Receiving Tube Manual RC-22, 6SN7GT: https://w5jgv.com/downloads/RC-22.pdf
- Mullard ECC82 / 12AU7: https://shop.ehx.com/catalog/tubes-specs/12AU7Mullard.pdf
- Mullard EL34: https://tubedata.tubes.se/sheets/129/e/EL34_Mullard.pdf
- RCA 6L6GC: https://www.robkalmeijer.nl/techniek/electronica/datasheets/6/6l6gc.pdf


## StrattTrack 1.2.8 — correlation confidence floor

The broadband normalized correlation estimate remains
`rho = E[LR] / sqrt(E[L²] E[R²])`, with 50 ms exponential averaging.
The negative-correlation side safeguard is now gated unless both channel
mean-square estimates exceed `1e-9` (−90 dBFS RMS for normalized float audio).
This prevents quantization residue and near-silence from being treated as a
reliable stereo relationship and spuriously attenuating Side. The gate does not
alter audible programme above the floor, the M/S matrix, Width law, mono sum,
or any parameter identity. It is a confidence floor, not a phase-alignment
algorithm or a replacement for listening to the mono fold-down.

### Hardware design references and target principles

- **SPL Stereo Vitalizer Mk2-T** — inter-channel phase-derived stereo expander; its
  manufacturer-published specifications include 10 Hz–100 kHz (−3 dB), 0.0019%
  THD+N, 121 dB dynamic range, −99 dBu A-weighted noise, balanced XLR/TRS I/O.
  Its manual explicitly recommends goniometer/mono checks at aggressive settings.
  https://spl.audio/de/alle-technischen-daten/?post_id=17918
  https://www.spl.audio/wp-content/uploads/stereo_vitalizer_mk2_9526_manual.pdf
- **Drawmer 1976** — three independent low/mid/high width and saturation bands,
  M/S width processing, 70 Hz–1.5 kHz and 800 Hz–15 kHz crossovers; manufacturer
  specs list 20 Hz–20 kHz ±0.2 dB, crosstalk below −75 dB at 1 kHz and 0.007% THD+N
  at 1 kHz (0 dB ref +4 dBu). Its manual warns that width changes band level and
  supplies band-level compensation.
  https://www.drawmer.com/products/70s_series/1976_three_band_saturation.php
  https://drawmer.com/uploads/manuals/1976_operators_manual.pdf
- **Dangerous Music S&M** — analog M/S encode/insert/decode matrix for separate
  centre and side processing; manufacturer specs: 1 Hz–100 kHz ±0.1 dB,
  THD+N below 0.002%, IMD60 4:1 below 0.002%, M/S separation −78 dB,
  dynamic range 108 dB, +23 dBu maximum input.
  https://www.dangerousmusic.com/product/s-m
- **Maselec MTC-1X** — mastering image section with an elliptical filter that
  folds non-correlated low-frequency content toward mono and a two-step stereo
  width control; its manual states that these image functions preserve the mono
  signal. The available manual copy is secondary-hosted, so values are not
  represented here as manufacturer-verified electrical specifications.
  https://studyres.com/doc/7801228/mtc-1x---maselec

### Mathematical and psychoacoustic acceptance criteria

Use the explicit M/S convention already used by StrattTrack:
`M=(L+R)/2`, `S=(L-R)/2`, `L'=M+wS`, `R'=M-wS`.
Therefore `(L'+R')/2=M` for every finite width `w`: widening must not change
the mono fold-down. Width 0% is mono, 100% is unity reconstruction, and 200%
doubles Side without inventing Side for a dual-mono input. The normalized
correlation coefficient is bounded to [−1,+1]; +1 means identical channels,
0 means uncorrelated channels, and −1 means polarity-opposed channels.
The input pan uses the equal-power law `gL=cos((p+1)π/4)`,
`gR=sin((p+1)π/4)`, so `gL²+gR²=1`. The polarity switch remains a discrete
0°/180° inversion committed at a zero crossing, with a bounded 1 ms fallback
for DC-like material; it is not described as continuous phase rotation.

Reference foundations:
- Wickelmaier & Choisel, AES, M/S-based stereo-width manipulation and listening tests:
  https://www.mathpsy.uni-tuebingen.de/wickelmaier/pubs/WickelmaierChoisel2005AES.pdf
- CMU, loudness concepts and equal-power panning laws:
  https://www.cs.cmu.edu/~music/icm-online/readings/panlaws/
- Audio Engineering Society, stereo imaging and perception literature:
  https://aes.org/
