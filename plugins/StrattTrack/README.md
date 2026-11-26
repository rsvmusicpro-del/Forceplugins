# StrattTrack

Stratt SSL-style stereo channel strip for Akai MPC / Force external effect hosting.

## Signal path
Stereo input → input trim → preamp → independent L/R polarity → true dual input pan (each input can be positioned independently across the stereo field) → 12 dB/oct Butterworth high-pass and low-pass → four-band parametric EQ (LF/HF shelves plus two bell bands) → linked-stereo gate → linked-stereo compressor → controllable circuit drive and fixed valve-inspired transfer (one shared colour block, parallel dry/wet) → output trim → stereo output balance → fader.

All stages outside Circuit are intended to be transparent: linear gain/pan, stable second-order filters and dynamics gain control. Circuit Mix at 0% bypasses the entire nonlinear colour path (both the adjustable rational Drive stage and the fixed asymmetric valve-inspired transfer); at 100% it selects the full colour path. No hidden valve coloration remains in the dry signal.

## Model and limits
The EQ uses RBJ cookbook biquads with block-rate coefficient updates driven by 10 ms control smoothing; HP/LP are second-order Butterworth sections. Gate and compressor use independent linked-stereo peak envelopes: gate attack/release controls its own detector and 3 dB hysteresis, while compressor attack/release controls the compressor detector and the conventional hard-knee amplitude-domain law `G = (level / threshold)^(-(1 - 1/ratio))`. Compressor gain follows its smoothed detector directly to avoid applying the same time constant twice; gate transitions use a separate 2 ms de-click slew. The Circuit engine owns the bounded rational waveshaper, fixed asymmetric valve-inspired transfer and 3 Hz DC blocker, then blends the complete colour path in parallel. At Circuit Mix = 0%, the colour path is mathematically bypassed. Filter state and colour/DC state are per-channel; linked dynamics state is isolated from both. This is a mathematical, tunable digital approximation, not a measured capture of an SSL console or a claim of exact component-level equivalence. No reference hardware measurements were supplied.

## Build and test
```sh
python tools/build.py plugins/StrattTrack
python tools/validate.py plugins/StrattTrack
python tools/package.py plugins/StrattTrack
```

The GitHub Actions workflow on `feature/stratttrack` builds native, ARM32 and ARM64, validates the deployable folder, and exercises the host simulator at multiple sample rates and block sizes.
