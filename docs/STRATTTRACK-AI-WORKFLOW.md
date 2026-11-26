# StrattTrack — autonomous, SDK-governed development

## Operating model

ChatGPT/coordinator owns scope, task decomposition, integration and final verification. Specialist agents may investigate and propose changes through isolated branches and pull requests; GitHub Actions is the independent build/test/package gate. This repository configuration supplies instructions and CI gates. It does not itself provision or activate Copilot Coding Agent, GitHub Agentic Workflows, model credentials, or additional GitHub permissions.

## Stages

1. **Coordinator intake** — define observable behavior, compatibility constraints, risks and acceptance evidence. Read `AGENTS.md`, this guide, SDK docs and the relevant agent skill.
2. **Read-only specialist analysis** — request independent DSP, SDK/ABI, QA and security reviews. Specialists return findings and test proposals; no shared-file concurrent writes.
3. **Implementation** — one designated writer works on an isolated `agent/stratttrack-*` or `copilot/stratttrack-*` branch. Pull requests target `feature/stratttrack`, never `main`.
4. **Independent review** — reviewers inspect the diff and run focused checks. The implementation agent cannot approve its own work. SDK/framework/workflow changes require owner review.
5. **CI gate** — run the contract/DSP tests, native + ARM32 + ARM64 build, UI generation, deployable-folder validation, host simulation across sample rates/block sizes, WAV render and installable ZIP packaging.
6. **Coordinator integration** — reconcile review findings, inspect the complete diff and rerun checks on the integrated commit. A green specialist report alone is not acceptance evidence.
7. **Release boundary** — no automatic merge to `main`, hardware deployment, device restart or public release. Those actions require an explicit human decision.

## SDK-aware acceptance checklist

- [ ] Root `AGENTS.md` and relevant `.agents/skills/` read and followed.
- [ ] AKAI host contract and SDK framework APIs verified; no guessed ABI.
- [ ] UID, parameter IDs/order, state compatibility and MPC Sample mapping preserved.
- [ ] Real-time callback free of allocation, locks, logging and I/O.
- [ ] Numerical, stability, sample-rate and variable-block behavior tested.
- [ ] No prohibited proprietary SDK/firmware dependencies; DSP provenance/licence recorded.
- [ ] Native, ARM32 and ARM64 outputs built.
- [ ] `tools/validate.py` passes on the deployable MPC folder.
- [ ] Host simulator has no FAIL; warnings are explained.
- [ ] ZIP package exists and includes the installable plugin folder and instructions.
- [ ] Review findings resolved; CI rerun on final PR head.

## Security model and limitations

- GitHub Actions jobs use explicit read-only repository permissions and receive no deployment credentials.
- Agents must work through PRs and must not receive PATs, signing keys, MPC network credentials or release secrets.
- Protected files are listed in `.github/CODEOWNERS`. CODEOWNERS requests review; it becomes a hard merge gate only if repository rules require code-owner approval.
- Branch rules, required status checks, Actions policy, Copilot access, Agentic Workflow engine credentials and secret/environment configuration are repository/account settings. They cannot be established by adding files alone and must be checked in GitHub settings.
- The current CI workflow builds and packages artifacts but does not deploy them to hardware.
- Agentic workflows are non-deterministic. Treat their output as untrusted code; CI and human review remain mandatory.
