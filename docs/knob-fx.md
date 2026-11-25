# MPC Sample: effects as Knob FX

The MPC Sample loads third-party effects as **Knob FX**: the insert effect that the MPC Sample's three knobs control. This page covers everything that differs from the other MPCs, and the `plugin.json` keys that set up the knobs.

## What the MPC Sample supports

| | MPC Sample |
|---|---|
| Firmware | **v1.4.0 or later** |
| CPU | 64-bit ARM: loads `file64` |
| Plugin kinds | **Effects only.** Instrument plugins are not supported. |
| Where effects appear | In the Knob FX list, after the built-in effects, sorted by name, with the "External plugin" icon. The Input FX list offers the same effects. |
| On-screen UI | **None beyond the Knob FX knobs.** There is no plugin edit screen: `Plugin Skins/` (`TUI.json`, `Q-Links.json`) and `ui.json` are not used. |
| Parameters the user can touch | **6**: 3 on the knobs, 3 more on the knobs while SHIFT is held |
| Enabling plugins | **MIDI Config**, in the **Dev Enhancements (Beta)** section: turn on **Dev Enhancements (Beta)**, then **Custom Plug-In Support** |
| Installing | Copy the plugin folder to `VST2 Plugins` on a microSD card through the MPC Sample's **SD Card Access** mode (see below) |

## Installing on the MPC Sample

The way to put plugins on an MPC Sample is its microSD card, opened on the computer through **SD Card Access** mode:

1. Put a microSD card in the MPC Sample.
2. Connect the MPC Sample to the computer with USB.
3. On the MPC Sample, open **MIDI Config** and, in the **Dev Enhancements (Beta)** section, turn on **Dev Enhancements (Beta)** and then **Custom Plug-In Support** (once; the setting is remembered).
4. From the **Project** screen, enter **SD Card Access** mode. The SD card appears as a drive on the computer.
5. Copy the whole plugin folder, `plugins/<Name>/build/<Manufacturer> - VST - <Name>/`, into a folder named exactly `VST2 Plugins` at the top level of the SD card (create `VST2 Plugins` if it is not there).
6. Eject the SD card on the computer. Once it is ejected, exit **SD Card Access** mode on the MPC Sample.
7. The plugin appears as a Knob FX at the bottom of the **FX Select** list.

To update a plugin, repeat steps 4–6 and replace the old folder with the new one. To remove it, delete its folder the same way. Always eject on the computer before leaving SD Card Access mode, or the copy may be incomplete.

Everything else in `host-contract.md` applies: same VST2 host, same audio format, threads, state and the parameter-0 nudge. The plugin folder still goes in `VST2 Plugins` at the root of a drive.

Every other parameter keeps working and is saved with the project, but on an MPC Sample nobody can change it. The host selects program 0 after loading the plugin, so a parameter that is not on a knob stays at program 0's value (or its default, if the plugin has no programs). Choose those values carefully, because on this device they are final.

## The knobs

```
              knob 1        knob 2        knob 3
 normal     knobs[0]      knobs[1]      knobs[2]
 SHIFT held shiftKnobs[0] shiftKnobs[1] shiftKnobs[2]
```

Each knob shows a label and the parameter's value text, and turns the parameter over its whole range (0 to 1 to the host). Knob pickup follows the MPC Sample's *Knob FX Takeover* setting. A small SHIFT indicator appears only when at least one SHIFT knob is assigned.

## plugin.json keys

```json
{
  "name": "Stereo Delay",
  "kind": "effect",
  "shortName": "St Delay",
  "knobs": [
    { "parameter": 2, "label": "Time" },
    { "parameter": 3, "label": "Feedback" },
    { "parameter": 7, "label": "Mix" }
  ],
  "shiftKnobs": [
    { "parameter": 4, "label": "Low Cut" },
    { "parameter": 5, "label": "High Cut" },
    { "parameter": 0, "label": "Sync" }
  ]
}
```

| Key | Type | Meaning |
|---|---|---|
| `shortName` | string, ≤ 12 characters | The name shown where there is little room. Without it, the first 12 characters of `name` are shown. It is shown as written, never cut. |
| `knobs` | array of up to 3 knob entries | Knobs 1, 2 and 3 in the normal state, in that order. |
| `shiftKnobs` | array of up to 3 knob entries | Knobs 1, 2 and 3 while SHIFT is held, in that order. |

A knob entry is an object:

| Key | Type | Meaning |
|---|---|---|
| `parameter` | integer ≥ 0 | The **0-based** parameter index: the position of the parameter's `addParam()` call in the constructor (the same index as the plugin's `ParamIndex` enum, and as `index` in `build/params.json`). |
| `label` | string | The knob's name on screen. |

The rules the MPC Sample follows:

- **Order is position.** Entry 0 is knob 1, entry 1 knob 2, entry 2 knob 3. Entries after the third are ignored.
- **Unassigned knobs.** A knob is unassigned (greyed out, showing `-`) when its entry is missing, is `{}`, has no `parameter`, has a `parameter` that is not a whole number (`"2"` or `2.0` do not count), or names a parameter the plugin does not have. To leave knob 2 empty but use knob 3, write `[{"parameter": 0, "label": "Drive"}, {}, {"parameter": 1, "label": "Mix"}]`.
- **Labels.** A knob without a `label` shows `Param <n>` with `n` counted **from 1** (parameter 0 shows `Param 1`). It does **not** fall back to the parameter's own name, so always write a label. Keep labels to about 8 characters: three knobs share the 320-pixel-wide screen.
- **Values.** The value text comes from the plugin, exactly as on the other MPCs: the framework's display text (with its unit) or your `formatValue()` override. Keep it to about 8 characters.
- **No mapping at all.** When `plugin.json` has neither `knobs` nor `shiftKnobs`, the knobs control parameters 0, 1 and 2, the SHIFT knobs parameters 3, 4 and 5, all labelled `Param 1` … `Param 6`. Parameter 0 is often master volume, so this is rarely what you want: always ship a mapping.
- **Only one of the two keys.** If only `knobs` is present, the SHIFT knobs are all unassigned and the SHIFT indicator is hidden. If only `shiftKnobs` is present, the normal knobs are all unassigned.

`tools/validate.py` checks all of this against the parameters the binary reports, and prints the mapping it resolved (`knob 1 -> [2] Time "350 ms"`) so an off-by-one shows up before the plugin reaches the device.

## Choosing the six parameters

- The 3 normal knobs get the parameters people reach for while playing: the effect's character and its amount (delay: time, feedback, mix; filter: cutoff, resonance, drive; distortion: drive, tone, mix).
- The 3 SHIFT knobs get the next most useful ones (tone shaping, width, a mode or sync switch). Leave SHIFT knobs unassigned rather than filling them with parameters nobody would touch.
- Bool and choice parameters work on a knob: the framework steps through the choices as the knob turns. A switch whose other state cannot be reached makes the knobs it controls useless (for example a delay Time knob while Sync is on), so put the switch on a knob too, or choose a default that suits the knob you expose.
- Design the plugin so the six knobs are the whole instrument on this device. A plugin built mainly for the MPC Sample is better with 6 parameters than with 20 hidden ones.

## Identity is permanent

The mapping refers to parameters by index, so it inherits the SDK's rule: **never reorder parameters once a plugin is released**; append new ones at the end. If you change which parameter a knob controls, users' muscle memory changes with it; change it only with a new version, and say so in the README.

## Testing

`host_sim` and `validate.py` work as for any effect. On the device: load the effect as Knob FX, turn each knob and check its label and value text, hold SHIFT and do the same, then save and reload a project.
