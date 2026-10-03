# Tonecoder TC-32 beta hardening / RC0

Base: main `413f84d`. Version 0.9.0; candidate label 0.9.0-beta.1.

## Bugs and root causes

- Note On test mixed pre-roll durations by using sample 2048 at every rate and
  assumed zero intrinsic synth onset. Reproduction: old residual onset after
  reported latency was +495 samples at 96 kHz and +49 at 192 kHz. A common 50 ms
  event removes this mismatch. The new test uses a resampled 48 kHz oracle,
  quantisation bounds, compensated onset and exact pre-event causality.
- SRC phase rounding could carry a near-1 fraction into a future source index.
  Clamp the phase without carrying; an adversarial-rate regression detects it.
- FILTER cutoff/resonance were created without a visual parent; parent them to
  FILTER NETWORK, retaining its manual layout.
- REL = DEC used JUCE ButtonAttachment's denormalised 0/1 writes on a 0–127 int
  parameter. Use ParameterAttachment with 0/127 writes and threshold 64.
- Factory session restore recaptured edited values as baseline; compare against
  the pure factory column. Persist USER baselines so later edits survive recall.
- Untrusted uiPreset could escape the factory index range; strictly normalize it.
- JSON arrays pass JUCE isObject() but lack DynamicObject (crash); enforce the
  actual object type. Validate the complete document before committing, clamp
  numeric values, reject incorrect/nonfinite types, default missing values,
  preserve legacy flat patches and emit versioned full patches.
- Keyboard rendering used a JUCE mutex/MidiBuffer insertion on the audio thread.
  Replace it with a bounded UI-to-audio FIFO, panic on overflow and mirror host
  keyboard display on the UI timer. Skip unsupported long SysEx before copying it.
- Graph listeners accessed GUI floats/repaint from parameter callbacks; poll
  atomics on message-thread timers. Protect file/menu callbacks with SafePointer.
- A recalled patch could retain a pending MIDI CC override. Cancel it by an atomic
  generation consumed on the audio thread. Prepare recreates DSP histories while
  preserving APVTS; transport stop releases held notes once.
- CC120 did not silence intentional infinite release or FX tails. Distinct panic
  resets envelope levels/effect history while retaining settings, without heap,
  locks or I/O. CC123 retains normal release semantics.
- Orphaned emsdk gitlink lacked .gitmodules; remove the tracked link and ignore
  local SDKs. Web CI already installs Emscripten independently.

## Validation

Local Windows x64 Release uses portable MSVC 2022 and JUCE 8.0.0. Factory validator:
156 checks PASS. CTest: 5/5 PASS (param_logic, midi_state, engine, resampler, plugin).
Plugin suite: 731259 checks PASS; SRC suite: 48090 checks PASS; engine suite: 51 checks
PASS, including independent fundamentals for all four simultaneous voices.

| Host Hz | Event sample (50 ms) | Onset | Reported SRC latency | Compensated error vs 48 kHz | Rates × 9 blocks / musical stream / C++ allocations |
|---:|---:|---:|---:|---:|---|
| 44100 | 2205 | 2296 | 88 | -15.306 µs | PASS / exact / 0 |
| 48000 | 2400 | 2404 | 0 | 0 µs | PASS / exact / 0 |
| 88200 | 4410 | 4592 | 175 | -3.968 µs | PASS / exact / 0 |
| 96000 | 4800 | 4998 | 191 | -10.417 µs | PASS / exact / 0 |
| 176400 | 8820 | 9184 | 351 | -9.637 µs | PASS / exact / 0 |
| 192000 | 9600 | 9996 | 382 | -10.417 µs | PASS / exact / 0 |

The musical stream includes envelopes/LFO/delay and equals the SRC-filtered 48 kHz
processor reference exactly. 48 kHz remains bit-transparent with zero SRC latency.
Blocks: 1/16/32/64/128/256/512/1024/2048. Tests also cover JSON/state corruption,
all parameter paths, MIDI performance, program timing, pressure/breath/velocity,
mode changes with held notes, panic, repeated editor lifecycle and prepare/recall.

Pluginval: pinned 1.0.4, strictness 5, GUI enabled, six rates and nine block sizes.
Final local VST3 run: SUCCESS, exit code 0, after panic/toggle fixes.
Remote CI evidence is attached to [PR #42](https://github.com/ovelhaaa/pra32-colorcoder/pull/42);
its mandatory checks must be green for the current head before approval. CI pins Windows 2022 / VS2022 x64,
checks download SHA256, waits for pluginval's actual process exit, and packages
VST3/Standalone only after successful CTest/pluginval.

## Changed files

- PluginProcessor.{cpp,h}: patch validation/baselines, MIDI queue/reset/lifecycle.
- PluginEditor.cpp, PRA32Components.{cpp,h}: FILTER parenting, toggle, safe callbacks,
  GUI-thread graph polling and parameter component IDs for smoke tests.
- PRA32Resampler.h, PRA32MidiState.h: source causality and short-message dispatch.
- pra32-u2-{eg,chorus-fx,delay-fx,synth}.h: explicit allocation-free CC120 panic.
- tests/{plugin_tests,engine_tests,resampler_tests}.cpp: temporal, parameter/UI,
  MIDI, lifecycle, rate/block, heap guard and four-voice spectral regressions.
- CMakeLists.txt, build-windows.yml, .gitignore, removed emsdk gitlink: version,
  toolchain, mandatory validation/artifacts and SDK cleanup.
- juce_plugin/README.md, TESTING.md and these audit documents: tester instructions,
  stable identity, JSON/dirty semantics and CC0 credits.

## Remaining limits and recommendation

Standard JUCE lifetime ownership requires closing/destroying the editor before its
processor. Intentionally destroying a processor while retaining an editor is a host
contract violation; no unsafe ownership workaround is introduced. GUI lifetime tests
and pluginval cover normal host ordering. Native FileChooser interaction and named
DAW session/transport smoke testing remain external tester work. The C++ allocation
guard covers operator new; it is not a general OS-level realtime instrumentation tool.
Intentional release=127 remains a held tail until panic; CC120 now terminates it.

Historical IDs PR32/Colr, APVTS IDs, factory values and layout identity remain stable;
bundle is com.Tonecoder.PRA32ColorcoderPlugin. PRA32-U2 / ISGK Instruments license is
CC0 1.0 Universal; JUCE's separate licensing remains applicable.

Recommendation: do not call this candidate ready until the final VST3 pluginval run
and mandatory GitHub CI are green. No release/tag or installer is created by this work.
