# StrattTrack DSP notes

## Shared HP/LP filter standard (1.1.2)

StrattTrack and StrattPultek use the same TwoPoleButterworth implementation from framework/dsp/two_pole_filter.h. Both use second-order Butterworth high-pass and low-pass sections (Q=1/sqrt(2)), RBJ bilinear-transform coefficients designed in double precision, a 10 ms logarithmic cutoff glide and 64-frame coefficient refresh. HP range is 20–1000 Hz and LP range is 1000–22000 Hz; coefficients are bounded to 0.45 × sample rate. Invalid input/output resets only the affected filter state. The sections are cascaded HP then LP before the Pultec/channel EQ stages.

## Signal path and algorithms
- Input/preamp: linear dB gain with smoothing and finite input guards.
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
- Four-band EQ: RBJ LF/HF shelves and LM/HM peaking sections, neutral at 0 dB.
- Dynamics: linked-stereo peak sensing, independent exponential gate/compressor detectors, 3 dB gate hysteresis, 2 ms gate gain slew, hard-knee compressor amplitude law and post-reduction makeup.
- Circuit: bounded rational drive, asymmetric valve-inspired transfer and 3 Hz DC blocker; complete nonlinear path is parallel-blended, so 0% is true clean bypass.
- Output: smoothed trim/fader and smoothed stereo balance.

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
channel-strip tone and dynamics stages and precedes the final output gain/fader.

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

Input/output trim, preamp, final fader and parallel circuit mix now use an explicit one-pole coefficient derived from the actual sample rate, giving each control the same 10 ms time constant at 44.1, 48 and 96 kHz (and other supported rates). The smoother API no longer has a hard-coded per-sample default, so future call sites must choose a rate-correct coefficient explicitly. Console Fader retains its intentional 1.5 ms response; block-rate controls retain elapsed-block 10 ms smoothing.

The StrattTrack contract test verifies the sample-rate-derived one-sample coefficient against the 10 ms response at 44.1, 48 and 96 kHz and enforces explicit coefficients at all gain-control smoother call sites.
