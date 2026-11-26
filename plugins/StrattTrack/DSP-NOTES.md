# StrattTrack DSP notes

## Signal path and algorithms
- Input/preamp: linear dB gain with smoothing and finite input guards.
- Polarity and pan: 10 ms block-compensated smoothing avoids abrupt polarity/pan discontinuities; equal-power input pan maintains constant summed source power.
- HP/LP: second-order Butterworth sections (Q=1/sqrt(2)), bilinear-transform coefficients, block-rate smoothing.
- Pulse Techniques EQP-1A stage (after filters, before four-band EQ):
  - LF boost and attenuation use first-order low shelves with selectable 20/30/60/100 Hz turnovers.
  - The attenuation turnover is offset by sqrt(2), matching the approximately half-octave higher corner visible in EQP-1A3 response sweeps.
  - A broad LF boost/attenuation interaction bell approximates the measured combined lift-and-dip contour; its depth is bounded and saturating, centred at 8× the selected LF frequency.
  - HF boost uses an RBJ peaking section at 3/4/5/8/10/12/16 kHz; Q maps from 1.60 (sharp) to 0.45 (broad).
  - HF attenuation is a first-order high shelf at 5/10/20 kHz.
  - Controls are smoothed in log-frequency or linear gain space; coefficients are calculated once per block; filter memory is per channel.
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
