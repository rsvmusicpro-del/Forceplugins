# Supported MPCs and how they differ

One plugin folder built with this SDK runs on every MPC listed here. The MPCs fall into three groups, and the group decides what your plugin can do on that device:

| Group | Models | Plugins it loads | Plugin UI |
|---|---|---|---|
| **32-bit MPCs** | MPC X, MPC X SE, MPC Live, MPC Live II, MPC One, MPC One+, MPC Key 61, MPC Key 37, Force | Instruments and effects | Touch screen pages and Q-Link knobs |
| **64-bit MPCs** | MPC Live III, MPC XL, and later models | Instruments and effects | Touch screen pages and Q-Link knobs |
| **MPC Sample** | MPC Sample | **Effects only** | **Six parameters on three knobs**, no plugin screen |

The 32-bit and 64-bit MPCs behave the same way towards a plugin; they differ in processor and in which library file they load. The MPC Sample is the odd one out: it is 64-bit, but it loads only effects and shows none of the touch UI.

## What every supported device has in common

- **The same plugin folder.** `"<Manufacturer> - VST - <Name>/"`, copied into a folder named exactly `VST2 Plugins` at the top level of a drive or card. You never build a separate version per model.
- **The same host.** Every device loads the plugin as a VST 2.4 shared library and calls it the same way: the same audio format, threading, state saving and parameter handling (`host-contract.md`).
- **Parameters are the interface.** Whatever the user can touch, automate or save is a parameter. The devices differ only in how many parameters they put in front of the user.
- **No plugin windows.** No MPC opens a plugin's own graphical editor. The UI is described by data files in the plugin folder.
- **Plugins are off until the user turns them on.** Each device has a setting that enables third-party plugins (below).
- **Effects receive no MIDI, and no plugin can send MIDI to the MPC.**

## 32-bit MPCs

**Models:** MPC X, MPC X SE, MPC Live, MPC Live II, MPC One, MPC One+, MPC Key 61, MPC Key 37 and Force.

**What they can do with a plugin**

- Load **instruments** (as plugin programs, played from the pads, keys or sequencer) and **effects** (as inserts).
- Show the plugin's **touch screen UI**: named pages on the function keys along the bottom of the screen, with knobs, faders and switches drawn from `Plugin Skins/TUI.json`. Skins are laid out for a 1280 × 800 screen, of which a page uses 1280 × 628.
- Control it from the **Q-Link knobs**, mapped per page by `Plugin Skins/Q-Links.json`. The knobs work in groups of four; touching a control selects its group and the MPC draws a focus rectangle around it.
- Edit a focused control with the data wheel, or double-tap it for a large edit overlay.
- Save user presets from the plugin's edit screen, automate every parameter, and save everything with the project.
- Show a usable default UI when the plugin ships no skins: pages of 16 sliders, with the knobs mapped in parameter order.

**Processor.** 32-bit ARM (ARMv7-A with NEON, Cortex-A17 class). This is the slowest group, roughly a 2013 phone, and it sets the CPU budget for every plugin: aim for a few percent of one core per instance, and stay well under 15% at 128-frame blocks. A plugin that runs comfortably here runs comfortably everywhere.

**Library loaded.** `VST/<slug>-arm32.so`, named by `file32` in `plugin.json`. Without `file32` the plugin is ignored on these models.

**Requirements.** MPC OS 3.10.1 or later. In **Preferences → Enhancements (Beta)**, turn on **Enable Enhancements** and **External Plugin Support**.

**Installing.** Copy the folder to `VST2 Plugins` on a USB drive or SD card, or send it over the network with `tools/mpc_remote.py` (needs the MPC's developer mode; see `install-and-test.md`).

**Differences inside the group.** The models differ in how many Q-Link knobs they have and how they are arranged, not in what a plugin can do. The MPC X has 16 knobs in a 4 × 4 block, which is the layout the UI pages mirror. Models with fewer knobs reach the same 16 assignments four at a time, one group per bank, which is why pages are designed in blocks of four. Force has its knobs eight in a row; a plugin can ship a variant mapping for it (`Q-Links - 8by1.json`), and the most specific matching file wins. The MPC Key 61 and Key 37 add a keyboard, which matters only in that instruments get played from keys as well as pads.

## 64-bit MPCs

**Models:** MPC Live III, MPC XL, and later 64-bit models.

**What they can do with a plugin.** Everything in the 32-bit list above: instruments and effects, the same touch screen pages, the same Q-Link mapping, presets, automation and project saving. The same `Plugin Skins/` files are used unchanged.

**How they differ from the 32-bit MPCs**

| | 32-bit MPCs | 64-bit MPCs |
|---|---|---|
| Processor | ARMv7-A, Cortex-A17 class | AArch64, Cortex-A55/A76 class |
| Library loaded | `file32` → `VST/<slug>-arm32.so` | `file64` → `VST/<slug>-arm64.so` |
| If that key is missing | Plugin ignored | Plugin ignored |
| CPU headroom | Least; design for this | More, but do not rely on it |
| Denormal handling by the host | Flush-to-zero on | Not on (the SDK framework flushes denormals itself on every device, so plugins built with it behave the same) |

**Requirements and installing.** The same as the 32-bit MPCs: MPC OS 3.10.1 or later, **External Plugin Support** on, and a drive or the network.

A plugin tuned only on a 64-bit MPC can overload a 32-bit one. Because one folder serves both, treat the 32-bit budget as the limit.

## MPC Sample

The MPC Sample is a 64-bit device that loads third-party **effects** as **Knob FX**: the insert effect its three knobs control. Full details are in `knob-fx.md`.

**What it can do with a plugin**

- Load **effects only**. Instrument plugins are not supported and do not appear.
- List each effect at the bottom of the **FX Select** list, after the built-in effects, sorted by name, with the "External plugin" icon. The Input FX list offers the same effects.
- Give the user **six parameters**: three on the knobs, and three more on the same knobs while SHIFT is held. Each knob shows a label and the parameter's value text.
- Save the effect's settings with the project.

**What it does not do**

- No plugin edit screen. `Plugin Skins/` and `ui.json` are not used, so nothing you design for the touch screen appears.
- No access to any parameter beyond the six. The others still work and are saved, but the user cannot change them; they stay at program 0's values (or their defaults).
- No installing over the network. `tools/mpc_remote.py` is not used with it.

**Which six parameters.** An effect's `plugin.json` names them:

- `knobs`: up to 3 entries, knobs 1 to 3.
- `shiftKnobs`: up to 3 entries, the same knobs with SHIFT held.
- `shortName`: up to 12 characters, the name shown in the FX list.

Each entry has a 0-based `parameter` index and a short `label` (9 characters; three knobs share a 320-pixel-wide screen). Without a mapping the knobs fall back to parameters 0 to 5 labelled `Param 1` to `Param 6`, which is rarely useful, so every effect built with this SDK ships a mapping. The mapping is as permanent as parameter order.

**Library loaded.** `VST/<slug>-arm64.so` (`file64`), the same file the other 64-bit MPCs load.

**Requirements.** Firmware v1.4.0 or later. In **MIDI Config**, in the **Dev Enhancements (Beta)** section, turn on **Dev Enhancements (Beta)** and then **Custom Plug-In Support**.

**Installing.** Through its microSD card: connect the MPC Sample to the computer with USB, enter **SD Card Access** mode from the Project screen, copy the plugin folder into `VST2 Plugins` on the card, eject the card on the computer, then leave SD Card Access mode.

## Side by side

| | 32-bit MPCs | 64-bit MPCs | MPC Sample |
|---|---|---|---|
| Models | X, X SE, Live, Live II, One, One+, Key 61, Key 37, Force | Live III, XL and later | MPC Sample |
| Instruments | Yes | Yes | **No** |
| Effects | Yes, as inserts | Yes, as inserts | Yes, as Knob FX (and Input FX) |
| Library | `file32` (arm32) | `file64` (arm64) | `file64` (arm64) |
| Touch screen pages (`TUI.json`) | Yes | Yes | Not used |
| Hardware knob mapping | `Q-Links.json`, 16 per page in groups of four | `Q-Links.json`, 16 per page in groups of four | `knobs` + `shiftKnobs` in `plugin.json`, 3 + 3 |
| Parameters the user can reach | All | All | 6 |
| UI when nothing is supplied | Pages of 16 sliders | Pages of 16 sliders | Parameters 0 to 5 as `Param 1` to `Param 6` |
| Minimum firmware | MPC OS 3.10.1 | MPC OS 3.10.1 | v1.4.0 |
| Setting that enables plugins | Preferences → Enhancements (Beta) → External Plugin Support | Preferences → Enhancements (Beta) → External Plugin Support | MIDI Config → Dev Enhancements (Beta) → Custom Plug-In Support |
| Install from a drive or card | USB drive or SD card | USB drive or SD card | microSD card in SD Card Access mode |
| Install over the network | Yes (`mpc_remote.py`) | Yes (`mpc_remote.py`) | No |
| Where the plugin appears | Browser: External Plugins | Browser: External Plugins | Bottom of the FX Select list |

## What this means when you design a plugin

- **An instrument** runs on the 32-bit and 64-bit MPCs and not on the MPC Sample. Design its pages and Q-Link groups; there is no Knob FX mapping to write.
- **An effect** runs everywhere. Design the touch pages for the MPCs with screens, and choose six parameters for the MPC Sample that make the effect complete on their own: the three people reach for while playing on the knobs, the next three on SHIFT. Pick defaults for every other parameter knowing that on the MPC Sample they cannot be changed.
- **Budget CPU for the 32-bit MPCs**, whichever device you test on.
- **Build once.** `python tools/build.py` produces both libraries, the skins and the validated `plugin.json`; the validator checks that each library is the right architecture and, for an effect, prints the Knob FX mapping.

Related pages: [overview.md](overview.md), [host-contract.md](host-contract.md), [knob-fx.md](knob-fx.md), [plugin-folder-format.md](plugin-folder-format.md), [install-and-test.md](install-and-test.md).
