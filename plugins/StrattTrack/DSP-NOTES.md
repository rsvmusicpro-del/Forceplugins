# StrattTrack DSP notes

- **Input/preamp:** linear dB gain stages; independent polarity inversion per channel.
- **Dual pan:** independent equal-power pan law for input L and input R. Each source is distributed to both output buses; this is not a balance knob.
- **Filters:** second-order Butterworth HP/LP, Q=1/sqrt(2), RBJ bilinear-transform coefficients. Coefficients are recalculated once per block.
- **EQ:** low/high shelving filters and two peaking bands using RBJ cookbook equations; gain controls are exactly 0 dB at neutral. Frequency, gain and Q controls use a 10 ms block-rate smoother before coefficient recalculation.
- **Dynamics engine:** linked-stereo peak sensing feeds two independent envelope followers. The compressor uses its own exponential attack/release detector and the conventional hard-knee amplitude-domain law `G = (level / threshold)^(-(1 - 1/ratio))`; gain follows that smoothed detector directly (no second identical smoothing stage), then post-reduction makeup is applied. The gate has a separate detector with its own attack/release, an adjustable attenuation floor and 3 dB opening/closing hysteresis. A short fixed 2 ms gain slew de-clicks gate state transitions without coupling its timing to compressor settings.
- **Circuit engine:** rational bounded waveshaper, fixed asymmetric valve-inspired transfer and 3 Hz DC blocker are one colour engine, independently stateful per channel. Circuit Mix blends the complete nonlinear path in parallel; 0% is a true clean-path bypass.
- **Output:** output trim, stereo balance and smoothed fader.
- **Realtime:** fixed-size state, no allocations/locks/logging/I/O in process(). Parameter reads and coefficient calculations occur once per block, except detector and filter state updates.

The stereo filters and colour/DC memories are channel-local; linked dynamics has independent shared compressor and gate envelopes so stereo channels cannot drift apart, while gate timing cannot be changed accidentally by compressor controls. All modules remain fixed-memory and real-time safe. This is a behaviourally plausible channel-strip model using public digital-filter and dynamics equations. It is not an exact SSL model: console revisions differ, and no physical reference sweeps or component values were provided.

## Mathematical verification and references

The CI contract test independently checks the equations used by the implementation:
- 2nd-order Butterworth low-pass magnitude is (1/\sqrt{2}) at its corner frequency (-3.0103 dB).
- A 0 dB RBJ peaking section is unity at DC, centre frequency and Nyquist.
- Equal-power pan satisfies (g_L^2+g_R^2=1) across the full pan range.
- The hard-knee compressor law leaves 1.5 dB above threshold for a 6 dB over-threshold input at 4:1.
- A one-pole envelope reaches (1-e^{-1}\) after one time constant; the DC blocker has a zero at DC.
- Static contract checks preserve the 32-parameter identity/order, the stereo signal-flow order, independent gate/compressor envelopes and the true 0% circuit bypass.

Reference material used for the mathematical design (no third-party DSP source code is copied):
- Robert Bristow-Johnson, *Audio EQ Cookbook*, published as a W3C Working Group Note: https://www.w3.org/TR/audio-eq-cookbook/
- D. Giannoulis, M. Massberg and J. D. Reiss, *Digital Dynamic Range Compressor Design—A Tutorial and Analysis*, JAES 60(6), 2012: https://aes.org/publications/elibrary/elibrary-page/?id=16354
- S. J. Orfanidis, *High-Order Digital Parametric Equalizer Design*, JAES 53(11), 2005: https://aes.org/publications/elibrary/elibrary-page/?id=13397
- MusicDSP RBJ cookbook reference: https://www.musicdsp.org/en/latest/Filters/198-rbj-audio-eq-cookbook.html

These references justify standard digital filter and dynamics mathematics; they do not provide measured component values for a particular SSL console. The circuit colour block therefore remains an explicitly tunable nonlinear approximation, not a claimed hardware clone.
