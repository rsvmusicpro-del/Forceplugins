# Giving your plugin a touch UI (the easy way)

Without any UI files, the MPC already gives your plugin a working screen: pages of 16 parameter sliders, hardware knobs mapped in order. That is enough to test a plugin. A real UI needs three things more: named pages, parameters grouped sensibly, and controls that look like what they do. The SDK's `ui.json` gives you those in a few lines; `tools/gen_ui.py` turns it into the MPC's own skin files (`Plugin Skins/TUI.json` and `Q-Links.json`), which `build.py` runs automatically.

For full control over the look (images, custom widgets, envelopes), read `ui-reference.md` and write the skin files yourself in `plugins/MyPlugin/skins/`; the build copies them verbatim and skips the generator.

## The screen

- Pages are function keys along the bottom of the screen; the page name is the key label (keep it short, 8 characters or fewer).
- Each page offers a 4 × 4 grid of control slots, matching the 16 hardware knobs (Q-Links) in their 4 × 4 arrangement on MPCs. The grid is a convenience of the simple `ui.json` layout, not a limit of the MPC: with a hand-written skin controls can go anywhere (see `ui-reference.md`).
- The hardware knobs work in **groups of four**. Touching a control selects its group, and the MPC draws a focus rectangle around that group on screen. That rectangle looks right only when the four controls it encloses sit together as one block, so lay pages out as blocks of four. A group is a column of four slots by default (controls *n*, *n*+4, *n*+8, *n*+12 in reading order, like the hardware knob columns); set `"qlinkGroups": "rows"` in `ui.json` to make it a row of four consecutive controls instead. The orientation is one choice for the whole plugin: design every related set as one group in that orientation. The generator draws each group's rectangle around its own slots with an equal gap on every side, so it never cuts into a control (`validate.py` warns if a hand-written skin's rectangle does). On products with fewer knobs (Force, MPC X) the mapping still works; the bank buttons select the group.
- Controls are listed in reading order: left to right, top to bottom. Leave a slot empty with `"empty"`.
- Each control shows the parameter's name and current value text from your code. Double-tap or Enter opens a large edit overlay.

## ui.json

```json
{
  "kit": "mpc-red",
  "pages": [
    {
      "name": "OSC",
      "controls": [
        { "param": "Wave",      "widget": "switch" },
        { "param": "Detune",    "widget": "knob" },
        { "param": "Octave",    "widget": "knob" },
        { "param": "Level",     "widget": "fader" },
        { "param": "Sub Level", "widget": "fader" },
        { "param": "PWM Depth", "widget": "knob", "visibleWhen": { "param": "Wave", "choice": "Square" } },
        "empty",
        { "widget": "text", "text": "Oscillator 2" }
      ]
    },
    {
      "name": "FILTER",
      "controls": [
        { "param": "Cutoff",    "widget": "knob" },
        { "param": "Reso",      "widget": "knob" },
        { "param": "Mode",      "widget": "switch" },
        { "param": "Env Amt",   "widget": "knob", "polarity": "bipolar" },
        { "param": "Attack",    "widget": "fader" },
        { "param": "Decay",     "widget": "fader" },
        { "param": "Sustain",   "widget": "fader" },
        { "param": "Release",   "widget": "fader" }
      ]
    },
    {
      "name": "LFO",
      "controls": [
        { "param": "Shape", "widget": "knob", "overlay": "menu" },
        { "param": "Sync",  "widget": "switch" },
        { "param": "Rate",  "widget": "knob", "visibleWhen": { "param": "Sync", "choice": "Off" } },
        { "param": "Div",   "widget": "knob", "overlay": "menu", "visibleWhen": { "param": "Sync", "choice": "On" } }
      ]
    }
  ],
  "style": { "background": "ff1e1d1e" }
}
```

| Field | Meaning |
|---|---|
| `pages[].name` | Function-key label. Up to 32 pages. |
| `pages[].background` | Optional: a PNG from the project's `assets/` folder (copied into `Plugin Skins/` by the build) drawn behind that page's controls, over the background colour. Size it to the 1280-wide touch screen area of the tab. |
| `pages[].controls[]` | Up to 16 entries. A plain string is a parameter **name** exactly as declared in code (`floatParam("cutoff", "Cutoff", ...)` → `"Cutoff"`), shown as a generic slider (so use the object form with a `widget` instead); the string `"empty"` leaves the slot blank. |
| `widget` | `"knob"` (film-strip knob with name and value), `"fader"` (horizontal film-strip fader under a name / value header), `"switch"` (toggle for bool parameters, 2–4 captioned segment buttons for choice parameters), `"slider"` (the default when no widget is given: the MPC's generic name + value bar, which looks plain beside kit widgets, so avoid it in a themed UI), `"text"` (a static heading; needs `text`), `"empty"`. |
| `polarity` | For `knob` and `fader`: `"auto"` (default), `"unipolar"` or `"bipolar"`. Bipolar strips fill from the centre. Auto picks bipolar when the parameter's default is the centre of its range and its value text reads as zero. |
| `overlay` | What double-tap / Enter opens: `"knob"` (big knob editor, default) or `"menu"` (a list; the default for `switch`, and the right choice for any long choice list). |
| `visibleWhen` | Show this control only while another parameter has a given value: `{"param": "Wave", "choice": "Square"}` or `{"param": "Sync", "index": 1}`. Works with choice and bool parameters. |
| `kit` | The film-strip theme for `knob` / `fader` / `switch`: `"mpc-red"` (assumed when absent), `"amber"`, `"cyan"`, `"lime"`, `"violet"`, `"mono"`, or `{ "theme": "...", "accent": "AARRGGBB" }`. See `ui-kit/README.md` and `ui-kit/preview.png`. |
| `style` | Optional ARGB colours: `background`, `accent` (focus ring; follows the kit colour unless set), `text`, `muted`. |
| `qlinkGroups` | `"columns"` (default) or `"rows"`: whether a Q-Link group of four (and its on-screen rectangle) is a column or a row of the 4 × 4 grid. One setting for the whole plugin. |
| `knobImage`, `knobFrames` | Optional: your own film-strip PNG from the project's `assets/` folder for `"knob"` widgets instead of the kit's. `tools/make_knob.py` can generate a plain one. |

Every bool and choice parameter you reference is given discrete steps on the data wheel and the overlay, using the choice labels from your code.

Parameters you leave out of `ui.json` are still reachable through automation and still saved; they are simply not on screen. A common pattern is to put rarely used settings on a last page called `MORE`.

## Choosing knob, fader, switch or slider

The kit exists so a page reads at a glance: a knob says "dial this in", a fader says "how much", a switch says "pick one". Use the same widget for the same kind of job on every page.

| Parameter | Widget | Why |
|---|---|---|
| On/off (bool) | `switch` | A pill toggle; the value text still says On / Off. |
| Mode with 2–4 options (filter type, waveform, routing) | `switch` | Segment buttons captioned with the choice names; one tap selects. |
| Mode with 5 or more options (note divisions, long wave lists) | `knob` + `"overlay": "menu"` | Segments would not fit; the themed knob steps through the choices with the Q-Link and a double-tap opens a menu that lists them all. |
| Tone and character: cutoff, resonance, drive, tune, detune, rate, depth, width | `knob` | Dialled by feel; the arc shows where you are in the range. |
| Anything centred on zero: pan, fine tune, pitch offsets, modulation amounts that go negative | `knob` (bipolar) | The arc grows from the centre, so zero is obvious. Auto-detected for centre-zero defaults; set `"polarity": "bipolar"` otherwise. |
| Levels and mixes: volume, level, mix, dry/wet, send | `fader` | "How much" reads best as a bar filling from the left. |
| Envelope stages (A, D, S, R) | four `fader`s in one row | A row of faders looks like the envelope it controls. |
| Time, size, feedback on delays and reverbs | `knob` | Range-y parameters that are dialled, not set. |
| Values where the text matters more than the position (note names, ratios, divisions) | `knob` + `"overlay": "menu"` | The knob shows the value text too, and the menu lists every value. |

Rules of thumb:

- **Do not use the generic `slider` in a themed UI.** The plain MPC bar (what a bare parameter name, or `"widget": "slider"`, gives you) looks out of place next to film-strip knobs, faders and switches. Every parameter you show gets a kit widget; the table above has one for every case, including long choice lists. `gen_ui.py` prints a WARNING naming each generic slider it finds and the widget to use instead. Keep the generic slider only for a plugin with no kit widgets at all.

- Top row of page 1: the four controls someone touches first (cutoff and resonance on a synth; time, feedback and mix on a delay), as knobs or faders, never switches.
- Group by signal flow: oscillators, filter, envelopes, modulation, effects, output. Put a section's switch (its mode) in the same row as the knobs it changes.
- Make each block of four mean something. The four knobs of a Q-Link group drive its four controls and the focus rectangle wraps them, so put one related set in each block: the four stages of an envelope, the cutoff / resonance / drive / key-tracking of a filter, the time / feedback / tone / mix of a delay. A block of four is a column under the default `"qlinkGroups": "columns"` and a row under `"rows"`; pick one for the whole plugin. Avoid splitting a section across two blocks, and avoid blocks that mix unrelated controls just to fill space; a deliberate `empty` slot or a `text` heading is better than a stray control.
- One theme per plugin. `mpc-red` matches the MPC; `amber` suits drums and analogue-style synths, `cyan` delays, reverbs and digital effects, `lime` and `violet` playful instruments, `mono` utilities. An `accent` colour makes the kit match a brand.
- Do not rely on colour alone; the MPC screen is small and often viewed from an angle. Short names and unit-bearing value text (`1.2 kHz`, `-6.0 dB`, `1/8T`) matter more than the graphics.

## Regenerating

`build.py` regenerates the skins on every full build unless `plugins/MyPlugin/skins/` exists. For a quick check without cross-compiling, run the generator directly:

```
python tools/gen_ui.py --params plugins/MyPlugin/build/params.json --ui plugins/MyPlugin/ui.json --out "plugins/MyPlugin/build/<Manufacturer> - VST - <Name>/Plugin Skins" --kit-cache plugins/MyPlugin/build
```

To go from generated to hand-written, copy `build/<folder>/Plugin Skins/` to `plugins/MyPlugin/skins/` and edit from there; the kit PNGs come with it.

## What the generator cannot do

Vertical faders, custom graphics beyond the kit, envelope displays, XY pads and sub-tabs. All are possible in the full format; see `ui-reference.md` and the hand-written section of `ui-kit/README.md`.
