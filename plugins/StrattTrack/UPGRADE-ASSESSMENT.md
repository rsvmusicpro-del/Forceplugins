# StrattTrack — DSP upgrade assessment (1.5.3)

This note separates techniques that are technically transferable from features that would be misleading, too expensive, or unsupported in the MPC effect contract. It is a design assessment, not a claim that every vendor feature has been implemented.

## Already adopted

- **Airwindows Console7 channel input:** the plugin already contains a clean-room adaptation of the Console7Channel input topology: 20 kHz resonant low-pass, Fader gain law, Spiral/Density 80:20 nonlinear transfer and ADAA evaluation. The adaptation is explicitly not bit-identical to Airwindows' plugin and does not reproduce its separate buss stage or mix-bus interaction.
- **Airwindows anti-aliasing philosophy:** retain bounded nonlinear drive, analytic antiderivative ADAA and a clear distinction between alias reduction and zero aliasing. The current upgrade also flushes subnormal Console7 recursive/ADAA memory on ARM.
- **RBJ / Waves-style graphic EQ refinement:** the existing 16-band EQ uses a gain-adaptive proportional-Q law; 1.5.2 hardens the digital Q inversion and coefficient normalization in double precision and rejects quantized unstable poles with second-order Jury/Schur conditions.
- **Console-strip stability:** sample-rate-aware smoothing, finite input/state guards, per-channel state, last-known-good filter coefficients, hysteretic gating and independent tests are retained.

## Transferable ideas and constraints

| Reference | Mature design idea | StrattTrack adaptation | Decision |
|---|---|---|---|
| Airwindows Console7 / Console8 | Anti-alias filtering, nonlinear curve shaping, explicit channel/sub/buss roles, analog-inspired signal interaction | Keep Console7 channel input and ADAA; a separate buss model is not equivalent inside a single stereo insert because it lacks the multi-channel summing context | Console7 channel adopted; buss interaction deferred |
| Waves F6 | Floating dynamic EQ bands, per-band compression/expansion, M/S operation and live spectrum analysis | A future dynamic-band mode could reuse the existing linked detector/smoothing patterns, but requires sidechain design, threshold/attack/release controls and substantial parameter/UI budget | Candidate; not silently approximated by static EQ |
| Waves GEQ | Classic versus proportional-Q response, analyzer-assisted operation and zero-latency IIR options | Proportional-Q and minimum-phase zero-added-latency processing are already present; live spectrum display depends on host UI capabilities and must not be faked with static artwork | Proportional-Q adopted; analyzer deferred |
| UAD nonlinear plug-ins | Internal high-rate processing and antialias filters for nonlinear stages | Consider 2x/4x oversampling only for the nonlinear drive/valve path, with polyphase filtering and measured alias/CPU comparisons at 44.1/48/96 kHz | Experimental candidate; ADAA remains current low-cost path |
| Brainworx bx_console / TMT | Channel-to-channel component tolerance variation, independent THD, gate hysteresis, stereo modes and detailed monitoring | A deterministic, opt-in channel-variation mode could be explored; random noise or arbitrary L/R mismatch by default would undermine recall, mono compatibility and phase consistency | No TMT/noise imitation in this upgrade |
| bx_console AMEK 9099 | Notch modes, Sheen/Glow shelf voicing, frequency-band audition, Mono Maker and width | Mono Maker/width principles already exist as Bass Mono, M/S Width and Mono Compatibility; band audition and alternate shelf voicing are possible later with dedicated UI/state design | Core stereo tools adopted; extras deferred |

## Engineering gates for future sound-quality work

1. Compare nonlinear paths with level-matched null and residual analysis; measure harmonic spectrum and alias energy using swept sines and multitone signals at 44.1, 48 and 96 kHz.
2. For oversampling, document passband ripple, stopband attenuation, latency, CPU on the slowest supported ARM target, and bypass/transition behavior before choosing a factor.
3. For EQ, sweep all 16 centres and extreme gains at every supported sample rate; validate stored float poles after quantization, centre gain, bandwidth symmetry, coefficient continuity and recovery from non-finite automation.
4. For dynamics, measure static transfer, attack/release time constants, stereo linking and gate chatter with independent reference calculations.
5. For all audio callbacks, preserve no-allocation/no-lock/no-I/O rules, variable blocks, finite-value containment, state/preset identity and deterministic reset.

## Provenance and licensing

The Airwindows Console7 source is MIT-licensed; the existing adaptation retains the upstream copyright/license notice and is not represented as an exact recreation. The Waves, Universal Audio and Plugin Alliance products above are used only as behavioural/product-design references: no proprietary source, coefficients, binaries, firmware or manuals' protected code are copied into StrattTrack. The plugin's own DSP remains clean-room implementation and its existing MIT project terms.

## References

- Airwindows Console7: https://www.airwindows.com/console7/
- Airwindows Console8 / Console family overview: https://www.airwindows.com/console/
- Airwindows repository and MIT licence: https://github.com/airwindows/airwindows
- Waves F6 / EQ product family: https://www.waves.com/plugins/equalizers
- Waves manuals: https://www.waves.com/downloads/manuals
- Universal Audio high-definition/antialiasing guidance: https://help.uaudio.com/hc/en-us/articles/15345509659156-UAD-Plug-Ins-and-High-Definition-Audio
- Universal Audio plug-in manuals: https://help.uaudio.com/hc/en-us/sections/4573072157460-UAD-Plug-In-Manuals
- Brainworx bx_console N: https://www.plugin-alliance.com/products/bx_console-n
- Brainworx bx_console AMEK 9099: https://www.plugin-alliance.com/products/bx_console-amek-9099
