# Tonecoder TC-32 — Milestone V1 visual identity

Base: main `62d95b3`, including merged RC0 PR #42. Work branch: `codex/tc32-v1-visual`.

## Visual system

Embedded Montserrat Regular/Bold (with the SIL Open Font License) matches the
family typography inspected in local Apollo, Nimbus and Bubbles sources. The
header puts TONECODER first and TC-32 in a compact instrument model badge. A
recessed program display preserves factory and USER / SESSION identification;
edited patches add explicit MODIFIED text and an amber edge marker. INIT, JSON
load/save and keyboard visibility retain their original callbacks and confirmations.

Graphite surfaces, warm readable ink, subtle boundaries and consistent module
padding replace fluted/chrome knobs, brushed textures and repeated module screws.
One satin encoder body uses a precise colored pointer, neutral scale ticks and a
bipolar center mark. Readouts keep the existing musician-facing formatters and
use stronger Montserrat weight. Mode keys use textual states, subdued accent
fills and a thin selected rule. Compact toggles keep ON/OFF state readable.

Section accents: warm OSC amber, FILTER teal, ENVS amber, MOD steel blue, FX
violet, CHARACTER muted red. Navigation uses a small lamp and bottom rule.
FILTER and ENVS share recessed analyzer wells, subtle grids and 1.5 px traces.
Curve formulas and parameter polling are unchanged. Graph and mode-key sizes
are bounded at large window sizes.

## Reusable primitives/styles

- `PRA32Theme::instrumentFont` and semantic branding/heading/label/value fonts.
- Shared graphite surface, text, border and section-accent tokens.
- `drawRaisedPlate`, `drawInsetWell`, `drawAnalyzerGrid` and `drawLamp`.
- `ModulePanel`: restrained boundary, 12 px horizontal padding and compact title.
- `ParameterKnob` / `paintInstrumentKnob`: common satin encoder and readout.
- `SteppedSelector`: compact textual mode-key grid using existing enum metadata.
- `ToggleSwitch`: small mechanical pushbutton treatment.
- `SectionSelector`: instrument navigation keys with active rule/lamp.
- `ValueReadout`: integrated program window and explicit edited status.

## Visual audit and screenshots

All six sections were rendered from the shipping JUCE editor and inspected at
960×620, 760×500 and 1600×1000. The audit corrected disappearing mixer knobs,
the incomplete narrow-window fallback, output-toggle crowding, small enum cells,
and the compressed large-window keyboard. No controls were removed.

| Page | Default | Minimum | Maximum |
|---|---|---|---|
| OSC | [960×620](visual-v1/OSC-960.png) | [760×500](visual-v1/OSC-760.png) | [1600×1000](visual-v1/OSC-1600.png) |
| FILTER | [960×620](visual-v1/FILTER-960.png) | [760×500](visual-v1/FILTER-760.png) | [1600×1000](visual-v1/FILTER-1600.png) |
| ENVS | [960×620](visual-v1/ENVS-960.png) | [760×500](visual-v1/ENVS-760.png) | [1600×1000](visual-v1/ENVS-1600.png) |
| MOD | [960×620](visual-v1/MOD-960.png) | [760×500](visual-v1/MOD-760.png) | [1600×1000](visual-v1/MOD-1600.png) |
| FX | [960×620](visual-v1/FX-960.png) | [760×500](visual-v1/FX-760.png) | [1600×1000](visual-v1/FX-1600.png) |
| CHARACTER | [960×620](visual-v1/CHARACTER-960.png) | [760×500](visual-v1/CHARACTER-760.png) | [1600×1000](visual-v1/CHARACTER-1600.png) |

Reproduce with the Release `pra32_plugin_tests.exe --capture-ui docs/visual-v1`
from the repository root. The optional capture mode uses component snapshots,
requires no audio device, and overwrites PNGs rather than appending to them.
Ordinary test execution is unchanged.

## Validation

Windows x64 Release, portable MSVC 2022 and JUCE 8.0.0:

- VST3, Standalone and test targets build successfully.
- Factory validator: 156 checks pass.
- CTest: 5/5 pass (parameter logic, MIDI state, engine, resampler, plugin).
- Added layout coverage checks embedded Montserrat and every parameter control's
  minimum size and containment at five sizes, including 1600×500 and 760×1000.
- Plugin suite: 732105 checks, zero failures.
- Pluginval 1.0.4: SUCCESS, exit 0, strictness 5 with GUI tests enabled;
  44.1/48/88.2/96/176.4/192 kHz and blocks 1/16/32/64/128/256/512/1024/2048.
- `git diff --check` passes.

## Functional safety

Production edits are restricted to editor/theme/widget painting and layout, plus
embedding font resources. Processor, engine, resampler, parameter metadata,
formatters, factory table/JSONs, MIDI and state/preset code are unchanged. APVTS
IDs, plugin IDs, ranges, defaults, CC mappings and program behavior are unchanged.
Existing attachments, toggle writes, graph timers, preset actions, confirmation
flows and UI persistence remain intact. Local regression/pluginval evidence does
not replace external beta testing in named DAWs or on different DPI displays.

## V1.1 recommendations

- Consider continuous text/control scaling at very large windows; V1 currently
  keeps typography stable and provides extra space while bounding mode keys and
  graphs. Validate any scaling policy at 125%, 150% and 200% desktop DPI.
- Gather beta feedback on minimum-size waveform keys and compact encoders before
  changing density or minimum window dimensions.

No new synthesis functionality or release publication is included.
