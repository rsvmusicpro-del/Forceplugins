# StrattTrack 1.1.1

Stratt SSL-style stereo channel strip for Akai MPC / Force external effect hosting.

## Signal path
Stereo input → input trim → preamp → smoothed L/R polarity and equal-power dual input pan → 12 dB/oct Butterworth high-pass and low-pass → Pulse Techniques EQP-1A response-curve model → four-band parametric EQ → linked-stereo gate → linked-stereo compressor → controllable circuit drive and fixed valve-inspired transfer → output trim → smoothed stereo balance → fader.

The dedicated PULTEC page exposes the EQP-1A controls: 20/30/60/100 Hz low boost and attenuation, 3/4/5/8/10/12/16 kHz high peak boost with sharp-to-broad bandwidth, and 5/10/20 kHz high shelving attenuation. The stage sits after HP/LP and before the four-band channel EQ. Control ranges follow Pulse Techniques' published specifications.

## DSP model and limits
The Pultec stage combines first-order low shelves, the measured higher attenuation turnover, a bandwidth-controlled RBJ peak section whose peak gain follows the measured sharp-versus-broad difference, a high-frequency attenuation shelf, and a broad low-frequency boost/attenuation interaction contour. The interaction term is a fitted digital approximation guided by Pulse Techniques' published EQP-1A specifications and Audio Precision response plots for the EQP-1A3. Pulse Techniques states that EQP-1A and EQP-1A3 use the same components and specifications. This is a curve-informed model, not a component-by-component passive-network simulation or a claim of bit-exact hardware identity: public curves are plotted sweeps rather than downloadable raw complex transfer data, so every intermediate knob position cannot be recovered exactly.

The four-band EQ uses RBJ cookbook biquads; HP/LP are second-order Butterworth sections. Input pan, polarity, Pultec controls, existing EQ controls, gate/compressor thresholds and ratios, makeup and circuit drive glide over a 10 ms control interval to reduce parameter-step artefacts. Dynamics values are smoothed in dB space before conversion to linear gain; the compressor lookup curve follows the smoothed ratio. Gate and compressor use independent linked-stereo peak envelopes, 3 dB gate hysteresis and the hard-knee amplitude-domain compressor law. Circuit Mix at 0% bypasses the entire nonlinear colour path. No allocation, locks or I/O occur in the audio callback.

## UI and visual identity
StrattTrack follows the StrattPultek visual standard: MPC amber kit and focus accent (#D89A48), charcoal background (#17191C), warm off-white text, muted grey labels, matching film-strip knobs, switches and faders, and the same 1280×628 panel background. The shared visual treatment is applied consistently to INPUT, FILTERS, PULTEC, EQ, DYNAMICS and OUTPUT pages. The OUTPUT page groups the existing circuit colour controls with the dedicated Output master trim, stereo Out Pan and final Fader, following the StrattPultek output-stage convention. These are existing DSP parameters; their stable parameter IDs and signal-path positions are unchanged. The background artwork is supplied as `assets/stratt-bg.png`, with editable vector source `assets/stratt-bg.svg`. Only presentation is shared; StrattTrack retains its own controls, signal path and DSP.

## Build and test
```sh
python tools/build.py plugins/StrattTrack
python tools/validate.py plugins/StrattTrack
python tools/package.py plugins/StrattTrack
```

The GitHub Actions workflow builds native, ARM32 and ARM64, validates the deployable folder, and exercises the host simulator at multiple sample rates and block sizes.


## Console emulation

The input Console stage is an attributed MIT-licensed adaptation of Airwindows Console7Channel, using its reference Fader law and channel transfer with polynomial ADAA for lower aliasing on MPC ARM hardware. It is not bit-identical to the original VST, and it does not claim to reproduce the separate Console buss summing/decode stage. See `DSP-NOTES.md`.
