# StrattTrack agent instructions

These instructions apply to every AI coding agent and human contributor working on StrattTrack.

## Authority and branch boundaries
- The active integration target is `feature/stratttrack`; never target or push to `main`.
- Work on a dedicated short-lived branch (`agent/stratttrack-*` or `copilot/stratttrack-*`) and submit a pull request to `feature/stratttrack`. Do not commit directly to the integration branch.
- One writer owns a given file or interface at a time. Parallel agents may investigate independently, but must not concurrently edit the same source, framework header, manifest, test, workflow, or generated artifact.
- Agents have no authority to deploy to hardware, restart an MPC, publish releases, or merge into `main`. Do not use secrets or request privileged tokens.

## SDK is a binding contract
Read the repository-root `AGENTS.md` and the relevant `.agents/skills/mpc-plugin-*/SKILL.md` before making changes. Treat the following as authoritative, in this order:
1. AKAI/MPC host contract and repository SDK documentation under `docs/`.
2. Public framework interfaces under `framework/`, especially `mpc_plugin.h`, `mpc_vst2_abi.h`, DSP and resource headers.
3. Existing examples and templates.
4. StrattTrack's implementation and tests.

Do not invent host callbacks, ABI behavior, parameter semantics, manifest fields, or device capabilities. If documentation and implementation conflict, stop and report the discrepancy rather than guessing.

## Compatibility invariants
- Preserve plugin UID `S4X3`, manifest identity, parameter IDs, parameter order, existing state/preset compatibility and the published ARM32/ARM64 library paths.
- Never reorder, rename, reuse, or remove an existing parameter ID. New parameters are appended only, and their host-visible defaults and migration implications are tested.
- Keep `plugin.json`, C++ PluginInfo, folder identity, UI labels and version metadata consistent.
- Respect the SDK's effect contract and MPC Sample Knob FX mapping (three primary plus three shift knobs, valid parameter indices).
- Do not fetch, copy, or depend on proprietary MPC firmware files, Steinberg VST SDK code, or incompatible-licensed code. Record provenance and licence for borrowed DSP.

## DSP and real-time safety
- No allocation, locks, logging, filesystem access, or I/O in the audio callback.
- No per-sample transcendental coefficient work; compute coefficients at block/control update boundaries.
- Handle variable host block sizes (32–512 frames), supported sample rates and denormal/non-finite inputs safely.
- Preserve channel isolation, latency and bypass behavior unless the task explicitly changes them.
- Validate numerical behavior independently; do not write tests that merely duplicate the implementation's formula.
- Explain CPU cost and any approximation; do not claim hardware identity from curve-fit or perceptual models.

## Required evidence before requesting integration
Run and report the focused relevant tests, then the complete StrattTrack pipeline:
```sh
python tools/test_stratttrack_contract.py
python tools/test_airwindows_console7.py
python tools/test_strattpultek_valve.py
c++ -std=c++11 -O2 -Wall -Wextra -Werror -Iframework tools/test_strattpultek_filters.cpp -o /tmp/test_shared_filters
/tmp/test_shared_filters
python tools/build.py plugins/StrattTrack
python tools/validate.py plugins/StrattTrack
.sdk-tools/host_sim plugins/StrattTrack/build/native/stratttrack.so test --seconds 1 --sr 44100 --block 128
python tools/package.py plugins/StrattTrack
```
Also exercise host_sim at 44.1, 48 and 96 kHz with 32, 128 and 512 frame blocks. A local agent report is not a substitute for fresh CI evidence. Never describe StrattTrack as ready for MPC installation unless native, ARM32, ARM64, validation, host simulation and packaging all pass.

## Protected SDK surface
Changes under `framework/**`, `tools/build.py`, `tools/validate.py`, `AGENTS.md`, `.github/**`, and SDK documentation are high-risk. Keep changes minimal, explain why a plugin-level solution is insufficient, include regression coverage, and expect repository-owner review through CODEOWNERS. Never weaken CI, remove checks, broaden permissions, expose secrets, or silently change the SDK contract to make a plugin build pass.

## Independent review roles
Use separate review passes (not competing writers):
- DSP reviewer: transfer functions, stability, aliasing, smoothing, sample-rate invariance and CPU.
- SDK/ABI reviewer: host contract, parameter/state compatibility, manifests and device architecture.
- QA reviewer: test coverage, simulator behavior, packaging and artifact completeness.
- Security reviewer: workflow permissions, untrusted input, dependency pinning, secret boundaries and protected paths.

The coordinator reconciles findings, owns integration, reviews the combined diff, and reruns CI after all changes. A reviewer must not approve its own changes.
