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

* **Synthesis Engine**: Based on [Digital Synth PRA32-U2](https://github.com/risgk/digital-synth-pra32-u2) by ISGK Instruments (MIT License).
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
