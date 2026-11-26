## StrattTrack 1.5.2

- Harden biquad coefficient updates with double-precision normalization and second-order Jury stability checks on stored float coefficients; rejected designs preserve the last known-good filter state.
- Evaluate adaptive graphic-EQ bandwidth-to-Q conversion and peaking-bell design in double precision, reducing rounding sensitivity near Nyquist.
- Preserve all 16 fixed centres, gain ranges, parameter IDs/order, preset compatibility and 64-frame coefficient refresh cadence.

## StrattTrack 1.5.1

- Replace linear interpolation in Q with a smoothstep gain-to-octave-bandwidth law, narrowing from 1.9 octaves toward each band's actual fractional-octave spacing as cut/boost increases.
- Convert bandwidth to Q with the RBJ bilinear-transform frequency-warp relation, and cap bandwidth against the available Nyquist margin so the top band remains finite and stable at 44.1/48 kHz.
- Add independent checks for bandwidth symmetry, monotonic focus, centre gain, half-gain bandwidth and pole stability across all sixteen bands at 44.1/48/96 kHz; preserve all parameter IDs/order and MPC Sample mappings.

## StrattTrack 1.5.0

- Upgrade the 16-band graphic EQ from fixed-Q to gain-dependent proportional-Q: subtle moves remain broad and larger cuts/boosts progressively focus, matching classic analogue graphic-EQ interaction while retaining symmetric boost/cut and exact unity at 0 dB.
- Keep the Pioneer DEH-P8850MP documented 16-band frequency layout, ±12 dB range, stereo-linked RBJ cascade, 10 ms smoothing, 64-frame coefficient refresh, minimum-phase response and zero added latency.
- Add independent proportional-Q curve, centre-gain and pole-stability checks across all sixteen bands and 44.1/48/96 kHz sample rates; bump StrattTrack to 1.5.0.

## StrattTrack 1.3.1\n\n- Harden the permanent hybrid triode ADAA stage: non-finite input clears its per-channel nonlinear/DC memory and emits silence for that sample; finite input is bounded to ±64 before antiderivative evaluation.\n- Add an independent numerical regression suite for the hybrid ECC83/6SN7 transfer, antiderivative derivative, ADAA divided differences, all branch knees, and short-interval quadrature stability.\n- Run the new regression suite in the StrattTrack GitHub Actions workflow; preserve all 52 parameter IDs/order, presets, UI and MPC Sample mappings.\n\n## StrattTrack 1.2.9

- Replace first-order Bass Mono Side extraction with a dedicated second-order Butterworth Side high-pass (12 dB/octave), leaving Mid and mono fold-down untouched.
- Refresh input/output Pan, Width, Invert L-R and Bass Mono controls every 64 frames using elapsed-frame 10 ms smoothing, preventing host-block-sized stereo-image automation steps.
- Preserve discrete zero-crossing Phase L/R polarity inversion, raised-cosine Mid-preserving L-R inversion, equal-power pan and the existing 50 parameter IDs/order, presets, UI and MPC Sample mappings.
- Document Maselec MTC-1X, Drawmer 1976, Dangerous Music S&M hardware design references and independent response/mono-invariance tests.

## StrattTrack 1.2.8

- Gate the negative-correlation Side safeguard below −90 dBFS RMS on either channel, preventing near-silence/quantization residue from being treated as a reliable stereo relationship.
- Preserve the 50 ms normalized correlation estimator, Mid/Side reconstruction, mono fold-down, Width range, pan law, polarity transition, all 50 parameter IDs/order, presets and MPC Sample mappings.
- Document hardware references, mathematical invariants, mono-compatibility acceptance criteria and the distinction between polarity inversion and continuous phase rotation.

## StrattTrack 1.2.6

- Refresh LF/LM/HM/HF RBJ biquad coefficients every 64 frames instead of once per host callback; advance their 10 ms smoothers using the actual chunk length to reduce zipper-like automation steps at large host block sizes.
- Preserve static EQ response, neutral settings, all 50 parameter IDs/order, factory programs, and MPC Sample 3+3 mapping.

## StrattTrack 1.2.4

- Remove the active −24…+24 dB Input trim and gain multiplication; Preamp becomes the only active user input-gain stage after Console7.
- Retain the former `input` ID as a hidden, zero-range, non-automatable compatibility tombstone at parameter index 1, preserving all 50 parameter indices, factory-program values and saved-state IDs.
- Remap MPC Sample SHIFT knob 1 to Preamp (parameter 2); keep Output, HP Freq and LP Freq mappings unchanged, as well as Drive and Circuit.
- Expand compressor gain-computer lookup to 1,024 fourth-power intervals over 1×–8,388,608× detector overdrive, preserving the hard-knee law and sub-0.2% interpolation error.
- Adopt the shared native MPC red UI kit and preserve the full six-page/Q-Link layout.
- Update INPUT Q-Link ordering, documentation and regression assertions for the compatibility slot, absence of the Input gain path and expanded compressor range.

# Changelog

## StrattTrack 1.2.5

- Precompute the 1 ms polarity-fallback coefficient and 20 ms zero-crossing timeout in `prepare()` using the validated sample rate.
- Correct the DSP notes: LF/HF channel EQ are peaking (bell) biquads, not shelves; add a contract assertion to prevent the implementation and engineering documentation drifting apart.
- Remove exponential evaluation and sample-rate-to-timeout conversion from the per-sample polarity-switch path; preserve zero-crossing behavior, fallback timing, parameter identity and DSP transfer.


## StrattPultek 1.9.5

- Move parameter 0 (Output) after the fixed nonlinear valve-colour stage so the MPC host's load-time parameter nudge changes level only, not saturation or harmonic content.
- Keep the dedicated Fader as the final signal-path multiplication.
- Add a regression contract check for the post-colour master-gain invariant.
- Preserve all 16 parameter IDs/order, factory programs, UI pages and MPC Sample 3+3 knob assignments.

## StrattPultek 1.9.4

- Guard every audio-path parameter read against NaN/Inf and clamp it to the declared control range before smoothing or selector conversion.
- Prevent malformed host automation from permanently contaminating smoother state or producing invalid frequency-selector indices; normal finite automation and the saved-project contract remain unchanged.
- Preserve parameter IDs/order, factory programs, UI pages and MPC Sample 3+3 knob assignments.

## StrattPultek 1.9.3

- Stabilized first-order ADAA for tiny intervals crossing the ±1/±2 polynomial knees with split five-point Gauss-Legendre integration, exact for the degree-eight asymmetric transfer on each segment.
- Added deterministic regression coverage for all four knees, both interval directions and widths from 1e-4 to 1e-10; retained the existing local Taylor and closed-form paths elsewhere.
- Preserved all parameter IDs/order, factory programs, UI pages and MPC Sample 3+3 knob assignments.


## StrattPultek 1.9.1

- Bound finite input samples to ±16.0 (24 dBFS) before recursive DSP to prevent pathological host values from overflowing filter or ADAA state while preserving normal programme headroom.
- Added final non-finite output containment with per-channel DSP-state recovery; NaN/Inf cannot propagate into the MPC mix bus.
- Preserved parameter IDs/order, presets, UI and MPC Sample knob assignments.

## StrattPultek 1.9.0

- Retained the first-order Cleansweep HP/LP topology and moved coefficient design to double precision with prewarped analog-prototype documentation.
- Added non-finite sample/state recovery to the recursive HP/LP filters and guarded incoming host samples before recursive DSP processing.
- Expanded filter verification across 44.1–192 kHz, -3 dB cutoff calibration, 6 dB/octave slopes, extreme cutoffs, long-run boundedness and NaN recovery.
- Preserved all 16 parameter IDs and indices, factory programs, MPC Sample 3+3 knob mappings, and the filter-before-EQ signal path.

All notable changes to the MPC Plugin SDK are recorded here, newest first.

## Unreleased (will become 1.1)

### Added

- **MPC Sample support: ability to build Knob FX.** Effect plugins run on the MPC Sample (firmware
  v1.4.0 or later, *Custom Plug-In Support* on); instruments do not. An effect's `plugin.json`
  gains `shortName`, `knobs` and `shiftKnobs`: the 3 + 3 parameters on the MPC Sample's knobs,
  which are the plugin's whole UI there. The effect template and StereoDelay include a mapping,
  the validator checks and prints it, and the deploy skill installs through the MPC Sample's
  microSD card in SD Card Access mode. See `docs/knob-fx.md`.
- **Optional background image per UI page**: a `ui.json` page can name a PNG (`background`)
  from `assets/`, drawn over the background colour and behind the controls.
- **`ui.json` `qlinkGroups`** (`"columns"` default, or `"rows"`): Q-Link group orientation for
  the whole plugin.
- New validator checks: Q-Link rectangles that cut through a control, run off the tab area or sit
  unevenly (also for hand-written skins).

### Changed

- Q-Link group rectangles are now computed from each group's controls with an equal 10 px gap on
  every side, instead of one fixed tall rectangle on every page that cut into knobs and switches.
- `gen_ui.py` warns, naming each control and the kit widget to use instead, when a UI that uses
  kit widgets still has generic sliders. Long choice lists use `knob` with the overlay `menu`
  (themed). Templates scaffold kit widgets (`knob`, `fader`, `switch`); SimpleSynth and
  StereoDelay no longer use sliders. Guidance now says never to use the plain slider in a themed UI.
- Skills and `AGENTS.md` ask after a clean build whether you want a test install on a standalone MPC,
  and must wait for a yes before installing, updating or removing anything. A clean build or an
  earlier yes is not permission.

### Fixed

- `gen_ui.py` warns when a page `background` image is missing from `assets/`.

## 1.0

First public release. An SDK for building instrument and effect plugins for
Akai MPC standalone hardware from a plain-language description, using an AI coding agent
(Claude Code or Codex), and installing them directly onto the MPC. Requires MPC OS 3.10.1 or
later with *External Plugin Support* enabled. Runs on MPC X, X SE, Live, Live II, One, One+,
Key 61, Key 37, Force (32-bit) and MPC Live III, MPC XL and later (64-bit).

- **Framework** (`framework/`): header-only `mpc_plugin.h` (parameters, MIDI, transport, state,
  programs), `mpc_vst2_abi.h` (clean-room description of the VST 2.4 binary interface),
  `mpc_resources.h`, and `dsp/` building blocks: oscillators, biquad/SVF/one-pole filters, ADSR,
  LFO, delay line, noise, smoothing, voice allocation, tempo helpers, MIDI CC and WAV helpers.
- **Tools** (`tools/`): `build.py` (one-command cross-compile for both MPC CPU families, plus
  native, with Docker support), `host-sim` (command-line host that behaves like the MPC, for
  testing on your computer), `validate.py`, `gen_ui.py` (scripted UI from `ui.json`),
  `package.py`, `mpc_remote.py` (install and manage plugins on an MPC over the network),
  `new_plugin.py`, `make_ui_kit.py` and `make_knob.py` (film-strip knobs, faders, switches).
- **UI**: the `ui.json` scripted UI system with pages, Q-Link mapping and a `ui-kit/` of
  unipolar/bipolar film-strip knobs, faders and switches in several colour themes.
- **Examples** (`examples/`): `SimpleSynth` (8-voice subtractive synth with presets),
  `StereoDelay` (tempo-synced effect) and `DrumSynth` (multi-output drum instrument).
- **Templates** for new instrument and effect projects, and JSON Schemas for `plugin.json` and
  `ui.json`.
- **Docs** (`docs/`): plugin folder format, host contract, building, install and test, DSP
  cookbook, UI guide and reference, presets, troubleshooting, FAQ and legal notes.
- **Agent skills** (`.claude/skills/`, `.agents/skills/`): new, dsp, ui, build, test, package,
  presets, deploy and troubleshoot, plus `AGENTS.md` / `CLAUDE.md`.


## StrattTrack 1.2.7 — hybrid valve voicing

- Refined the permanent valve coloration stage with a normalized 72/28 ECC83/12AX7-like and 6SN7-like triode curve blend, retaining unity small-signal gain and the same stage position.
- Added exact closed-form hybrid ADAA antiderivative plus five-point Gauss–Legendre handling for short intervals split at both nonlinear knees and zero.
- Added subtle 18 kHz filtering of the nonlinear residual only; retained the 3 Hz DC blocker, parameter IDs/order, state contract and MPC Sample Q-Link mapping.
- No new user parameter or UI control; DSP notes document equations, valve electrical references, assumptions and limitations.


## StrattTrack 1.3.0 — selectable Mono Compatibility

- Added Mono Compatibility Off/Auto/Full and an adjustable Auto correlation threshold to the OUTPUT page.
- Auto progressively reduces M/S Side below the selected negative-correlation threshold; Full removes Side immediately before the permanent valve stage. The protection is smoothed and preserves Mid.
- Appended the two new parameters after the existing 50 slots; retained prior parameter indices, factory-program values and MPC Sample mappings.


## StrattTrack 1.4.0 — fixed 16-band graphic EQ

Replaced the active four-band parametric EQ path and EQ-page controls with sixteen fixed-frequency stereo-linked graphic bands (20 Hz–20 kHz, ±12 dB, constant-Q bell filters). Existing EQ parameter slots remain inert at their original indices for saved-session/automation compatibility; sixteen gain-only parameters are appended. Minimum-phase IIR, 10 ms smoothing and 64-frame coefficient refresh avoid linear-phase latency and audio-thread allocation. Research and provenance: `plugins/StrattTrack/GRAPHIC-EQ-RESEARCH.md`.
