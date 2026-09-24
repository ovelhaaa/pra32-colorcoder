# Milestone B — Host Robustness, Resampling & Sample-Rate Qualification

Scope: make the Tonecoder TC-32 wrapper robust as a VST3/Standalone instrument at
host sample rates other than 48 kHz, without touching the musical character of
the embedded PRA32-U2 engine. No new musical features were added.

## 1. Architecture

```
PRA32 engine @ 48 kHz  (unchanged DSP)
        |
        v
  on-demand engine pull  (synth.process per engine sample)
        |
        v
  windowed-sinc polyphase resampler  (wrapper output only)
        |
        v
  host sample rate (44.1 / 48 / 88.2 / 96 / 176.4 / 192 kHz)
```

* The engine is still authored and executed at a fixed **48000 Hz**.
* The resampler lives entirely in the wrapper (`PRA32Resampler.h`), is
  framework-free, and is regression tested standalone against the real engine.
* At exactly 48 kHz the resampler is a **bit-transparent passthrough**: one
  `synth.process()` call per host sample, with no phase accumulator, no history
  and no filtering.
* State (fractional read position + sample history) persists across
  `processBlock()` calls, so there are no block-boundary artefacts.
* The engine samples are synthesised on demand, so the resampler adds **zero
  algorithmic latency**; `setLatencySamples(0)` is asserted in `prepareToPlay`.

## 2. Resampler parameters

| Property | Value |
|---|---|
| Filter | Kaiser-windowed sinc, polyphase coefficient bank |
| Taps | 192 (96 per side) |
| Phases | 512 |
| Kaiser beta | 8.0 (~80 dB stopband) |
| Upsampling cutoff | 0.5 cycles/engine-sample (engine Nyquist) |
| Downsampling cutoff | `0.5 * ratio * 0.97` (stays below the new Nyquist) |
| Coefficient memory | 512 × 192 × 4 B = 384 KiB, allocated once in `prepare()` |
| History ring | power-of-two, `kTaps + 8` requirement |
| Runtime allocation / locks / I/O | none (all in `prepare()`) |

Per-phase coefficients are normalised to unity DC gain so every fractional
position has flat low-frequency response and uniform amplitude.

Anti-aliasing strategy: for downsampling (48 k → 44.1 k) the cutoff is placed so
the Kaiser transition band lands below the host Nyquist, giving ~89 dB of alias
suppression (versus ~7 dB for the old linear stage). For upsampling the cutoff
is at the engine Nyquist; the images sit far above it and are suppressed by the
sinc kernel's stopband.

## 3. Latency

Zero additional latency. The wrapper owns the engine, so for each host sample it
can generate the (few) future engine samples the kernel needs *after* applying
any MIDI event for that sample. A MIDI event applied before host sample *n* is
already reflected in the interpolated value for sample *n*. No host lookahead is
required and the plugin reports `setLatencySamples(0)`.

## 4. Measured results

### Pitch (autocorrelation, parabolic peak)

| Note | 44.1 kHz | 48 kHz | 96 kHz | 192 kHz | cross-rate spread |
|---|---|---|---|---|---|
| A4 (69) | 440.289 Hz | 440.188 Hz | 440.151 Hz | 440.141 Hz | **0.58 cent** |
| C4 (60) | 261.349 Hz | 261.365 Hz | 261.409 Hz | 261.397 Hz | **0.40 cent** |

The engine's own table quantisation offsets absolute pitch by ~1 cent; the test
asserts cross-rate consistency < 2 cents (the resampler never transposes).

### Timing

* Note-onset time across rates: max error **0.017 ms** (well under one host
  sample at 44.1 kHz).
* LFO period across rates: max error **0.17 ms** (~1.4665 s).

### Spectral quality

| Check | New | Old linear |
|---|---|---|
| 48 k → 44.1 k, 23 kHz alias | **−89.1 dB** | −7.1 dB |
| 48 k → 96 k, 10 kHz interpolation error | **−121.3 dB** | −19.7 dB |
| 48 k → 192 k, 10 kHz interpolation error | **−117.7 dB** | −19.5 dB |
| 1 kHz passband amplitude (all rates) | 0.000 dB | — |

### CPU (4 voices + chorus + delay, release build)

| Rate | Time / 1 s audio | RT factor |
|---|---|---|
| 44.1 kHz | 0.015 s | ~68× |
| 48 kHz | 0.008 s | ~122× |
| 96 kHz | 0.022 s | ~45× |
| 192 kHz | 0.036 s | ~28× |

### Tail / silence

* `getTailLengthSeconds()` now returns **20.0 s** (was `0.0`).
  Derived from the engine's own limits: max delay ≈ 0.34 s × ~10 repeats to
  −60 dB (≈3.4 s) plus the release time constant at CC 126 (≈17 s to −60 dB).
  CC 127 maps to an exactly non-decaying release coefficient, so no finite value
  can be perfect; 20 s is a documented conservative bound.
* Silence / note-off tails are finite and free of NaN/Inf for delay off, delay
  medium, high feedback and long release.

## 5. Files

New:

* `juce_plugin/Source/PRA32Resampler.h`
* `juce_plugin/tests/resampler_tests.cpp`
* `juce_plugin/tests/plugin_tests.cpp`
* `docs/milestone-b-report.md` (this file)

Modified:

* `juce_plugin/Source/PluginProcessor.h` — replaced linear-SRC state with
  `pra32::Resampler`.
* `juce_plugin/Source/PluginProcessor.cpp` — `prepareToPlay`, `releaseResources`,
  `renderAudioRange`, `getTailLengthSeconds`.
* `juce_plugin/CMakeLists.txt` — list the new header.
* `juce_plugin/tests/CMakeLists.txt` — new `pra32_resampler_tests` and
  `pra32_plugin_tests` targets.
* `.github/workflows/build-windows.yml` — build the new targets.

## 6. Tests

* `pra32_resampler_tests` (framework-free, 45 checks): 48 kHz bit-transparency,
  block-boundary continuity at 16…2048 samples, pitch/timing invariance, spectral
  anti-aliasing and interpolation quality vs the old linear stage, tail
  finiteness/decay, and an approximate CPU report.
* `pra32_plugin_tests` (JUCE console app): `getStateInformation`/
  `setStateInformation` roundtrip for every parameter, JSON `savePresetToJson`/
  `loadPresetFromJson` roundtrip (save → modify → load, plus idempotence),
  block-boundary host automation reaching the engine, and first-render-after-
  restore using the restored patch.
* Existing Milestone A/A.1 tests (engine, MIDI state, parameter logic) still run.

## 7. Automation model (documented explicitly)

* **MIDI CC / Program Change**: sample-accurate, applied inside the block at the
  exact event sample (`PRA32MidiState::dispatchSampleAccurate`).
* **Host parameter automation / GUI**: applied at **block boundaries** at the
  top of `processBlock` via `updateEngineFromParameters()`.

This split is unchanged from Milestone A and is acceptable for this milestone.

## 8. Residual risks

* The 48 kHz fast-path tolerance is 1 mHz; a host reporting a rate within that of
  48 kHz is treated as passthrough (error is far below audibility).
* Phase quantisation (512 phases) bounds interpolation error to about −120 dB;
  it is not exact for irrational ratios.
* Envelope release CC 127 is intentionally non-decaying, so any reported tail is
  a convention rather than a true bound.
* The local environment could not build JUCE (MinGW GCC 14 pragma failure /
  outdated Direct2D headers); the JUCE-based `pra32_plugin_tests` target is
  therefore validated by the MSVC CI job. The framework-free tests were built
  and run locally.
