# StrattTrack 16-band graphic EQ — design research

## Hardware references

| Reference | Type / construction | Fixed centres / documented treatment | Relevant characteristics |
|---|---|---|---|
| Rane GQ 15 | Dual-channel rack graphic EQ; 15 sliders/channel, gain, low/high cut, bypass and level indicators | 25, 40, 63, 100, 160, 250, 400, 630, 1k, 1.6k, 2.5k, 4k, 6.3k, 10k, 16k Hz | ±12 dB slider range; conventional constant-band graphic EQ |
| Pioneer DEH-P8850MP | Integrated digital audio processor/head unit with fixed 16-band graphic EQ | 20, 31.5, 50, 80, 125, 200, 315, 500, 800, 1.25k, 2k, 3.15k, 5k, 8k, 12.5k, 20k Hz | ±12 dB graphic range; separate front/rear/sub processing; Auto EQ variant +6 to −12 dB |
| Klark-Teknik DN332 | Dual-channel analogue 2/3-octave rack graphic EQ; engineering specification lists 16 centres/channel (current product overview describes 15-band coverage); 2U chassis | 20, 31, 50, 80, 125, 200, 315, 500, 800, 1.25k, 2k, 3.15k, 5k, 8k, 12.5k, 20k Hz | ±12 dB; proprietary MELT thick-film filter circuits; 18 dB/oct subsonic filter at 30 Hz; 20 Hz–20 kHz ±0.5 dB response; THD+N <0.01% at 1 kHz/+4 dBu; noise below −90 dBu; +22 dBu max output; balanced XLR, detented sliders, bypass and ground-lift provision |
| Phonic GEQ1500 | Dual-channel 1U rack analogue graphic equalizer | 15 sliders/channel, nominal 25 Hz–16 kHz | Input gain, output LED metering and channel controls; comparison model, not a 16-band EQ |
| JB Systems GEQ215 | Dual-channel 1U analogue graphic EQ | 25, 40, 63, 100, 160, 250, 400, 630, 1k, 1.6k, 2.5k, 4k, 6.3k, 10k, 16k Hz | ±12 dB, balanced XLR/jack I/O, >95 dB S/N, 20 Hz–20 kHz response, bypass and 50 Hz low cut; 15 bands |

The Klark-Teknik DN332 engineering specification and Pioneer DEH-P8850MP manual are the strongest directly documented 16-band references found: both cover 20 Hz–20 kHz with approximately 2/3-octave spacing and ±12 dB-class control. There is a source nuance worth retaining: Klark-Teknik's current DN332 product overview describes the spectral coverage as a 15-band-per-channel design, while its engineering specification explicitly calls for 2×16 ISO centre frequencies; the specification sheet is the evidence for the 16-point list. The Pioneer manual explicitly confirms the exact StrattTrack frequency set. The DN332 specifies ±0.5 dB flat response, THD+N below 0.01%, noise below −90 dBu and +22 dBu maximum output; its proprietary MELT thick-film circuitry is not reproduced. Neither product publishes full internal filter coefficients or circuit topology, so StrattTrack adopts public frequency/control conventions rather than claiming a hardware clone. Rane GQ15, Phonic GEQ1500, JB Systems GEQ215 and ART HD-15 are respected adjacent 15-band analogue references, not 16-band equivalents. The selected centres follow the Pioneer DEH-P8850MP manual and the familiar rounded preferred-number progression.

## Software references and design lessons

- **Voxengo Marvel GEQ (freeware, proprietary binary):** 16 bands, ±12 dB, 20 Hz–20 kHz, logarithmic centres, continuous interpolated response, linear-phase FIR, freehand curve drawing, stereo/multichannel and M/S grouping, 9 ms compensated latency. Its guide notes reduced low-frequency precision due to finite FIR kernel length. This is the closest 16-band behaviour reference; its linear-phase FIR/latency architecture is not copied.
- **Voxengo Overtone GEQ (freeware, proprietary binary):** seven bell bands, ±12 dB, channel groups, M/S/multichannel routing and optional harmonic enhancement/oversampling; adjacent bell gains can produce a combined response greater than a single slider's setting. Harmonic modules are not copied because StrattTrack has a separate valve stage.
- **CamillaDSP GraphicEqualizer (open source, GPL-3.0):** arbitrary band count, logarithmically spaced centres, same relative bandwidth and one peaking biquad per band. Architectural reference only; no source code imported.
- **ANWIDA GEQ31V (commercial, $49 at the cited product listing):** 31 fixed ISO 1/3-octave constant-Q bands from 20 Hz to 20 kHz, up to ±12 dB, high-precision transparent DSP. Paid reference for classic constant-Q response and slider behaviour; band count differs.
- **Waves GEQ (commercial):** 30 ISO-centred bands, selectable classic proportional-Q or modern interaction-reduced topology, ±18 dB, analyzer and zero-latency operation. Paid reference for the design trade-off between classic band interaction and modern smoother graphic response; band count differs.
- **Volko Audio QG (commercial):** 10-band API-560-inspired fixed-frequency graphic EQ, one-octave spacing, ±12 dB, proportional-Q and optional upsampling/clickless parameter updates. Paid reference for proportional-Q behaviour, not a 16-band target.
- **RBJ Audio EQ Cookbook:** peaking filter coefficient equations; StrattTrack uses its existing clean-room biquad implementation.

## StrattTrack transfer design

Fixed centres (Hz): 20, 31.5, 50, 80, 125, 200, 315, 500, 800, 1250, 2000, 3150, 5000, 8000, 12500, 20000.

Each slider controls one stereo-linked peaking/bell biquad, −12 to +12 dB, default 0 dB. Its gain-adaptive law is expressed in octave bandwidth: `BW(g)=BWbroad+(BWnarrow−BWbroad)·s(t)`, `t=clamp(|g|/12,0,1)`, `s(t)=t²(3−2t)`, `BWbroad=1.9` octaves and `BWnarrow` is the local spacing derived from geometric midpoints between adjacent centres. Q is derived using the RBJ bilinear-transform equation `Q=1/[2 sinh((ln 2/2)·BW·ω0/sin ω0)]`, `ω0=2πf0/Fs`; this corrects frequency warping at high centres instead of applying one analogue Q to every sample rate. The available bandwidth is capped by the Nyquist margin, `BW≤2 log2(Fs/(2f0))`, so the 20 kHz control remains bounded at 44.1/48 kHz even though its ideal symmetric upper/lower span cannot fit above Nyquist. Equal-magnitude cuts and boosts have identical bandwidth; 0 dB is exactly unity. Minimum-phase IIR processing and serial cascade preserve zero added latency; 10 ms gain smoothing and 64-frame coefficient refresh avoid zipper-like parameter steps. The audio callback performs no allocation, locking or I/O, and the EQ does not add an FFT/analyser or linear-phase latency. Adjacent bands interact naturally in the cascade and their combined response may exceed the gain shown by an individual slider.

The existing biquad safety clamp limits each centre to 0.45×sample-rate, preventing unstable coefficients near Nyquist. The former four-band parameter IDs and indices remain inert compatibility slots; their filters and EQ-page controls are removed. Sixteen gain-only controls are appended after the original 52 parameters to preserve existing project/automation indices.

## What working engineers expect and how they use a fixed graphic EQ

A useful fixed-band EQ is judged by repeatability and speed, not by the number of features: exact, permanently labelled centres; a known gain range and predictable band width; smooth, click-free automation; a trustworthy flat reference; sufficient internal headroom; no unexplained level jump; stable stereo linking; and a bypass/A-B path that allows level-matched comparison. Centre-detented faders and a visually readable frequency curve are valuable on analogue hardware because the control surface itself communicates the response. For a software implementation, coefficient stability at all supported sample rates, finite-state recovery, deterministic preset recall and low CPU cost are equally important.

Typical engineer workflow:

1. Listen first and identify the audible problem; use a spectrum/RTA or measurement sweep to locate candidates, not to decide the mix by itself.
2. Remove rumble or unusable subsonic energy with a suitable high-pass filter before spending graphic-EQ bands.
3. Prefer restrained cuts for broad room/speaker peaks, boxiness, mud, honk and harshness; large boosts consume headroom and can amplify noise.
4. Avoid trying to fill deep acoustic nulls with EQ: they are often caused by cancellation and require placement, timing, phase or room treatment rather than large boosts.
5. Use fixed graphic bands for broad tonal shaping and familiar fast moves; use a parametric EQ or narrow notch for a very narrow ring, feedback mode or surgical resonance that falls between fixed centres.
6. Recheck transients, vocal intelligibility, sibilance, cymbal noise and bass translation after each group of moves; audition at matched loudness and check mono compatibility.
7. For stereo programme material, linked L/R gains preserve tonal balance; independent channel correction is a different task and needs per-channel measurement and controls.

StrattTrack's current 16-band section meets the fixed-centre, ±12 dB, stereo-linked, minimum-phase, zero-added-latency, smoothed-automation and flat-at-zero requirements. Its flat reference is all sixteen gains at 0 dB; the plugin does not currently expose a separate graphic-EQ bypass switch or built-in spectrum analyser. The section is a broad musical/corrective tool, not a substitute for a narrow parametric notch or per-channel room-correction system.

## Frequency-use regions

| Region | Typical range | Common use / caution | Closest StrattTrack centres |
|---|---:|---|---|
| Sub / rumble | 20–60 Hz | Remove inaudible rumble; preserve kick/808 fundamentals and headroom | 20, 31.5, 50 Hz |
| Weight / punch | 60–200 Hz | Kick and bass body; avoid boom | 80, 125, 200 Hz |
| Mud / warmth | 200–500 Hz | Reduce cloudiness; preserve useful body | 200, 315, 500 Hz |
| Box / honk | 300 Hz–1.5 kHz | Tame cardboard, nasal or telephone tone | 315, 500, 800, 1.25 kHz |
| Presence / intelligibility | 1–3.15 kHz | Improve articulation carefully; ear sensitivity makes excess tiring | 1.25, 2, 3.15 kHz |
| Pain / harshness | 2–5 kHz | Find and reduce aggressive resonances; context dependent | 2, 3.15, 5 kHz |
| Sibilance / edge | 5–10 kHz | Vocal esses and cymbal splash; de-essing may be more appropriate | 5, 8 kHz |
| Air / sheen | 10–20 kHz | Gentle openness; excess emphasizes hiss/noise | 12.5, 20 kHz |

These are listening zones, not universal correction targets: instrument, microphone, arrangement, playback level and room change the perceived problem frequency. References include iZotope's instrument EQ guide and Audient's frequency-management overview.

## Sources

- Rane GQ15/GQ30 manufacturer datasheet: https://www.ranecommercial.com/legacy/pdf/old/gqdat.pdf
- Klark-Teknik DN332 official product overview (notes 15-band wording): https://www.klarkteknik.com/en/products/0821-ABN
- Klark-Teknik DN332 engineering specification (2×16 ISO centres, MELT, I/O, noise, THD, headroom): https://device.report/m/788b0808486590d7bf534724d7c93fbdb5e5e33e031cf07dec0162956366bbb3.pdf
- Pioneer DEH-P8850MP manual, 16-band centres, ±12 dB range, custom curves and front/rear/sub auto-EQ: https://www.elinfor.com/pdf/Pioneer/DEH-P8850MP_Pioneer.pdf
- Phonic GEQ1500/GEQ3100 manufacturer cut sheet: https://www.phonic.com/index.php/download-center/category/6-english-cutsheets?download=75%3Ads-geq1500-geq3100-en-pdf&start=60
- JB Systems GEQ215 specifications: https://musicdelivery.net/en/products/jb-systems-geq215
- Voxengo Marvel GEQ: https://www.voxengo.com/product/marvelgeq/
- Voxengo Marvel GEQ user guide: https://www.voxengo.com/files/userguides/VoxengoMarvelGEQ_en.pdf/getbyname/Voxengo%20Marvel%20GEQ%20User%20Guide%20en.pdf
- Voxengo Overtone GEQ: https://www.voxengo.com/product/overtonegeq/
- Voxengo Overtone GEQ user guide: https://www.voxengo.com/files/userguides/VoxengoOvertoneGEQ_en.pdf/getbyname/Voxengo%20Overtone%20GEQ%20User%20Guide%20en.pdf
- CamillaDSP: https://github.com/HEnquist/camilladsp
- Robert Bristow-Johnson, Audio EQ Cookbook (exact digital bandwidth/Q relation and peaking-biquad coefficients): https://www.w3.org/TR/audio-eq-cookbook/
- Välimäki & Reiss, “All About Audio Equalization: Solutions and Frontiers” (fractional-octave and Q relationships): https://www.mdpi.com/2076-3417/6/5/129
- Audiotec Fischer, DSP Special, §B.7 Equalizer (measure and reduce peaks; avoid large boosts): https://www.audiotec-fischer.de/media/pdf/a6/74/06/ATF_Sound_Tuning_Magazine-DSP_Special_Vol1_Englisch_web.pdf

No third-party DSP code is copied. Voxengo binaries are used only as behavioural references; CamillaDSP is GPL-3.0 and is neither linked nor copied into this MIT-licensed plugin.

- ANWIDA GEQ31V product specifications and pricing: https://www.anwida.com/as_plugins/geq31v/
- Waves GEQ product overview: https://www.waves.com/plugins/geq-graphic-equalizer
- Volko Audio QG product listing and technical overview: https://soundsspace.com/index.php/product-details/qg


## Adaptive bandwidth/Q refinement (1.5.1)

The former straight-line interpolation between Q=0.707 and Q=2.146 was replaced by a smoothstep interpolation in octave bandwidth, followed by the exact RBJ digital bandwidth-to-Q conversion. This makes the control law explicit in the same unit engineers use to describe graphic-band spacing, respects the slightly non-uniform rounded centre frequencies, and compensates for bilinear-transform frequency warping as each centre approaches Nyquist. The 20 kHz band is bandwidth-limited when the sample rate cannot represent a symmetric upper skirt; it is not allowed to generate an invalid or unstable pole pair. Independent tests cover all sixteen centres, both gain polarities, monotonic focus, exact centre gain, half-gain bandwidth and pole stability at 44.1, 48 and 96 kHz.
