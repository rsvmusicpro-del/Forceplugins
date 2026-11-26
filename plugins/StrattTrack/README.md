# StrattTrack

Stratt SSL-style stereo channel strip for Akai MPC / Force external effect hosting.

## Signal path
Stereo input → input trim → preamp → independent L/R polarity → true dual input pan (each input can be positioned independently across the stereo field) → 12 dB/oct Butterworth high-pass and low-pass → four-band parametric EQ (LF/HF shelves plus two bell bands) → linked-stereo gate → linked-stereo compressor → controllable circuit drive and fixed valve-inspired transfer (one shared colour block, parallel dry/wet) → output trim → stereo output balance → fader.

All stages outside Circuit are intended to be transparent: linear gain/pan, stable second-order filters and dynamics gain control. Circuit Mix at 0% bypasses the adjustable Drive/Saturation path. A fixed, subtle valve-inspired transfer remains continuously active inside the Circuit module; it has no exposed control and is not independently bypassable. The only colour-producing module is Circuit.

## Model and limits
The EQ uses standard RBJ cookbook biquads. HP/LP are second-order Butterworth sections. Gate and compressor use linked peak detection, exponential attack/release envelopes and conventional dB-domain threshold/ratio laws. The drive stage is a bounded rational waveshaper with a fixed asymmetric component and a 3 Hz DC blocker. This is a mathematical, tunable digital approximation, not a measured capture of an SSL console or a claim of exact component-level equivalence. No reference hardware measurements were supplied.

## Build and test
```sh
python tools/build.py plugins/StrattTrack
python tools/validate.py plugins/StrattTrack
python tools/package.py plugins/StrattTrack
```

The GitHub Actions workflow on `feature/stratttrack` builds native, ARM32 and ARM64, validates the deployable folder, and exercises the host simulator at multiple sample rates and block sizes.
