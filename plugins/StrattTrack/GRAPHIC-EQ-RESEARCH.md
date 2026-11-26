# StrattTrack 16-band graphic EQ — design research

## Hardware references

| Reference | Type / construction | Fixed centres / documented treatment | Relevant characteristics |
|---|---|---|---|
| Rane GQ 15 | Dual-channel rack graphic EQ; 15 sliders/channel, gain, low/high cut, bypass and level indicators | 25, 40, 63, 100, 160, 250, 400, 630, 1k, 1.6k, 2.5k, 4k, 6.3k, 10k, 16k Hz | ±12 dB slider range; conventional constant-band graphic EQ |
| Pioneer DEH-P8850MP | Integrated digital audio processor/head unit with fixed 16-band graphic EQ | 20, 31.5, 50, 80, 125, 200, 315, 500, 800, 1.25k, 2k, 3.15k, 5k, 8k, 12.5k, 20k Hz | ±12 dB graphic range; separate front/rear/sub processing; Auto EQ variant +6 to −12 dB |
| Phonic GEQ1500 | Dual-channel 1U rack analogue graphic equalizer | 15 sliders/channel, nominal 25 Hz–16 kHz | Input gain, output LED metering and channel controls; comparison model, not a 16-band EQ |
| JB Systems GEQ215 | Dual-channel 1U analogue graphic EQ | 25, 40, 63, 100, 160, 250, 400, 630, 1k, 1.6k, 2.5k, 4k, 6.3k, 10k, 16k Hz | ±12 dB, balanced XLR/jack I/O, >95 dB S/N, 20 Hz–20 kHz response, bypass and 50 Hz low cut; 15 bands |

The documented fixed 16-band example provides a logarithmic 20 Hz–20 kHz spread. Rane and other classic analogue rack units use a closely related 15-band family from 25 Hz to 16 kHz. The selected frequencies use the exact published Pioneer DEH-P8850MP 16-band set; they also closely match the standard rounded preferred-number progression used in fixed graphic equalizers. This is not a claim to reproduce its DSP hardware.

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

Each slider controls one stereo-linked peaking/bell biquad, −12 to +12 dB, default 0 dB. All bands use constant Q=2.146 (approximately 2/3-octave relative bandwidth), minimum-phase IIR processing and serial cascade. Gain changes are smoothed over 10 ms; coefficients refresh at no more than 64-frame intervals. There is no added latency, FFT, allocation, lock or I/O in the audio callback. Adjacent bands interact naturally in the cascade and their combined response may exceed the gain shown by an individual slider.

The existing biquad safety clamp limits each centre to 0.45×sample-rate, preventing unstable coefficients near Nyquist. The former four-band parameter IDs and indices remain inert compatibility slots; their filters and EQ-page controls are removed. Sixteen gain-only controls are appended after the original 52 parameters to preserve existing project/automation indices.

## Sources

- Rane GQ15/GQ30 manufacturer datasheet: https://www.ranecommercial.com/legacy/pdf/old/gqdat.pdf
- Pioneer DEH-P8850MP manual, 16-band EQ specifications: https://www.manualslib.com/manual/129935/Pioneer-Super-Tuner-Iii-D-Deh-P8850mp.html?page=77
- Phonic GEQ1500/GEQ3100 manufacturer cut sheet: https://www.phonic.com/index.php/download-center/category/6-english-cutsheets?download=75%3Ads-geq1500-geq3100-en-pdf&start=60
- JB Systems GEQ215 specifications: https://musicdelivery.net/en/products/jb-systems-geq215
- Voxengo Marvel GEQ: https://www.voxengo.com/product/marvelgeq/
- Voxengo Marvel GEQ user guide: https://www.voxengo.com/files/userguides/VoxengoMarvelGEQ_en.pdf/getbyname/Voxengo%20Marvel%20GEQ%20User%20Guide%20en.pdf
- Voxengo Overtone GEQ: https://www.voxengo.com/product/overtonegeq/
- Voxengo Overtone GEQ user guide: https://www.voxengo.com/files/userguides/VoxengoOvertoneGEQ_en.pdf/getbyname/Voxengo%20Overtone%20GEQ%20User%20Guide%20en.pdf
- CamillaDSP: https://github.com/HEnquist/camilladsp
- Robert Bristow-Johnson, Audio EQ Cookbook: https://www.w3.org/TR/audio-eq-cookbook/

No third-party DSP code is copied. Voxengo binaries are used only as behavioural references; CamillaDSP is GPL-3.0 and is neither linked nor copied into this MIT-licensed plugin.

- ANWIDA GEQ31V product specifications and pricing: https://www.anwida.com/as_plugins/geq31v/
- Waves GEQ product overview: https://www.waves.com/plugins/geq-graphic-equalizer
- Volko Audio QG product listing and technical overview: https://soundsspace.com/index.php/product-details/qg
