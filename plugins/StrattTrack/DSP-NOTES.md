# StrattTrack DSP notes

- **Input/preamp:** linear dB gain stages; independent polarity inversion per channel.
- **Dual pan:** independent equal-power pan law for input L and input R. Each source is distributed to both output buses; this is not a balance knob.
- **Filters:** second-order Butterworth HP/LP, Q=1/sqrt(2), RBJ bilinear-transform coefficients. Coefficients are recalculated once per block.
- **EQ:** low/high shelving filters and two peaking bands using RBJ cookbook equations; gain controls are exactly 0 dB at neutral.
- **Gate:** linked-stereo peak detector, dB threshold and adjustable attenuation floor, exponential attack/release.
- **Compressor:** linked-stereo peak detector, conventional hard-knee static curve and exponential attack/release; makeup is post gain reduction.
- **Circuit:** rational bounded waveshaper plus fixed asymmetry and DC blocking, blended with the clean signal by Circuit Mix. The fixed valve-inspired transfer is deliberately grouped inside the only nonlinear colour block; it is not a second independent colour stage.
- **Output:** output trim, stereo balance and smoothed fader.
- **Realtime:** fixed-size state, no allocations/locks/logging/I/O in process(). Parameter reads and coefficient calculations occur once per block, except detector and filter state updates.

This is a behaviourally plausible channel-strip model using public digital-filter and dynamics equations. It is not an exact SSL model: console revisions differ, and no physical reference sweeps or component values were provided.