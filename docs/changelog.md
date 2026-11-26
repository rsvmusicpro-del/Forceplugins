# Changelog

## StrattTrack 1.1.1

- Remove the redundant Input trim parameter and its gain stage completely; Preamp is now the first input-page control and the only input gain stage.
- Preserve all original VST parameter indices with an inert, non-automatable legacy slot at the retired Input position; the active Input control and gain stage are fully removed.
- Restore the original 40-slot factory program layout and correct MPC Sample knob indices (Preamp 2, Drive 28, Circuit 29).
- Add regression assertions for the compatibility tombstone, stable parameter indices, factory programs and the absence of any Input DSP gain path.


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
