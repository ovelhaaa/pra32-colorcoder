# Tonecoder TC-32

**Four-Voice Polyphonic Signal Synthesizer** (VST3 / Standalone)

Powered by the **PRA32-U2** synthesis engine.

---

## Overview

**Tonecoder TC-32** is a virtual instrument plug-in (VST3) and standalone synthesizer derived from the PRA32-U2 digital synthesizer architecture. It features a bespoke control panel designed for immediate musical workflow, packaged inside a physical instrument identity inspired by classic 1960–1980s laboratory test equipment, broadcast consoles, and vintage studio signal processors.

The design philosophy favors dark brushed metal, fluted bakelite controls, engraved technical typography, discreet jewel status lamps, and a precision **signal-path color identification system**:

* **OSC** (Amber) — Dual oscillator sources, shape, morph, drift, wave mode, and mixer stage.
* **FILTER** (Cyan / Teal) — Resonant low-pass / high-pass filter network with live frequency-response display and envelope shaping.
* **ENVS** (Olive) — Dedicated 4-stage Mod Envelope, Amp Envelope, and Pitch Modulation routing.
* **MOD** (Steel Blue) — Versatile Modulation LFO with multi-destination routing and Performance portamento/pitch bend controls.
* **FX** (Copper) — Stereo chorus processor, stereo/ping-pong delay network, and output gain/pan staging.
* **CHARACTER** (Mauve) — Voice allocation (Poly, Mono, Legato), dynamic response (velocity sensitivity, aftertouch mod), and breath control.

---

## Synthesis Engine Architecture

The TC-32 sound engine is based on the 4-voice polyphonic PRA32-U2 engine:

- **Dual Oscillators**: Waveforms including Sawtooth, Square, Triangle, Sine, Wavetable, Pulse, Noise, and Sync/Ring modulation behaviors.
- **Waveform Shaping & Morphing**: Dynamic shape modulation with continuous morphing and analog-style oscillator drift.
- **Mixer**: Balance between Oscillator 1 and 2, plus dedicated Noise / Sub-oscillator generation.
- **Filter Network**: 2-pole resonant multimode filter (Low Pass / High Pass) with key tracking and breath modulation.
- **Dual Envelopes**: Mod EG and Amp EG with natural exponential curves.
- **Modulation LFO**: 6 LFO waveforms (Triangle, Sine, Noise, Sawtooth, S&H, Square) assignable to pitch, cutoff, or shape.
- **Effects Network**: Built-in stereo chorus and stereo / ping-pong delay effects.
- **Performance & Voice Character**: Polyphonic, Monophonic, and Legato voice allocation modes; velocity sensitivity, aftertouch routing, and MIDI breath control.

---

## Technical Credits & License

* **Synthesis Engine**: Based on [Digital Synth PRA32-U2](https://github.com/risgk/digital-synth-pra32-u2) by ISGK Instruments (CC0 1.0 Universal; see the repository LICENSE and [upstream license](https://github.com/risgk/digital-synth-pra32-u2/blob/main/LICENSE)).
* **Tonecoder TC-32 UI & Plug-in Implementation**: Built with JUCE 8.

---

## Building the Plug-in

Requirements:
- CMake 3.20 or newer
- C++20 compliant compiler (MSVC 2022 on Windows, Clang on macOS, GCC/Clang on Linux)
- Git (for fetching JUCE)

```bash
cd juce_plugin
cmake -B build -G "Visual Studio 17 2022"  # or Ninja / Unix Makefiles
cmake --build build --config Release
```

Build targets:
- `Tonecoder TC-32_VST3`
- `Tonecoder TC-32_Standalone`


## RC0 / beta hardening

Product version: 0.9.0; candidate package label: 0.9.0-beta.1.
Stable historical identity: COMPANY_NAME `Tonecoder`, PRODUCT_NAME `Tonecoder TC-32`,
manufacturer `PR32`, plugin `Colr`, bundle `com.Tonecoder.PRA32ColorcoderPlugin`. Existing APVTS IDs and their version hints are
unchanged. The CMake target name is intentionally retained. JUCE remains pinned
to 8.0.0; Windows CI uses Windows 2022, VS2022, x64, Release.

Patch JSON schema 1 is `{ "schemaVersion": 1, "parameters": { "FILTER_CUTOFF": 112, ... } }`.
The legacy flat object (including `[value]` / `[[value]]` containers) remains readable.
Both formats are **full patches**: missing parameters use `SynthParameters` defaults
(INITIALIZATION), unknown fields are ignored, numeric values are rounded and clamped
to each parameter's real range. Invalid root/schema/known parameter types or nonfinite
values reject the entire document without changing the current patch.

A factory session always compares against its pure factory column; edits remain
modified on recall. A loaded JSON patch starts a USER baseline. That baseline is
saved in session XML and restored, including subsequent edits. Legacy USER sessions
without baseline metadata adopt their restored values as baseline. Invalid `uiPreset`
metadata is normalized to -1 (USER/SESSION).

Windows CI runs CTest and pinned pluginval 1.0.4 at strictness 5, with GUI tests enabled,
all six rates and nine block sizes. Strictness 5 is pluginval's documented minimum
host compatibility level: https://github.com/Tracktion/pluginval . No validation step
is optional; plugin archives are created/uploaded only after successful validation.
Diagnostic logs may be uploaded on failure. Packages include BUILD.txt with commit SHA.
See [TESTING.md](../TESTING.md) for tester instructions.

PRA32-U2 / ISGK Instruments is credited under CC0 1.0 Universal. JUCE is separately
licensed under its commercial terms or AGPLv3; the engine's CC0 license does not
relicense JUCE. Distribution must follow the project's applicable JUCE license.

The web workflow installs its own Emscripten SDK using setup-emsdk. The orphaned
local emsdk gitlink is removed; local SDK folders are ignored and are not deleted.
