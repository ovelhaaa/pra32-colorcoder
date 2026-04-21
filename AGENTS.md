# Repository purpose

This repository contains a PRA32-U2-derived embedded synthesizer project targeting RP2350 / Arduino-Pico, with a control-panel/UI layer and a synth/audio engine that must remain stable.

The repository is being migrated from an older control-panel model to a new one based on:

- 1 rotary encoder with push click
- 1 ST7789 color display
- 284x76 widescreen layout
- embedded-friendly rendering and interaction model

This migration must happen in bounded phases.

---

## Non-negotiable constraints

### Preserve audio behavior

Do not break or regress:

- synth voice behavior
- MIDI input behavior
- control-change semantics
- program read/write semantics
- sequencer timing and transport behavior
- panel play and seq mode behavior
- audio stability / glitch resistance

### Preserve parameter semantics

Do not change parameter meaning, controller numbering, or value ranges unless explicitly required and documented.

### Preserve page-table-driven structure

The page/group/parameter organization is defined centrally and should remain the source of truth.

### Prefer conservative engineering

- Do not use flashy or risky UI techniques.
- Do not add unnecessary architectural novelty.
- Do not introduce heavy redraw loops or fragile automation.

---

## Important files

Key existing files include:

- `Digital-Synth-PRA32-U2/Digital-Synth-PRA32-U2.ino`
- `Digital-Synth-PRA32-U2/pra32-u2-control-panel.h`
- `Digital-Synth-PRA32-U2/pra32-u2-control-panel-page-table.h`

These files currently contain panel/UI/input/display behavior and should be treated carefully.

---

## Migration philosophy

The migration is intentionally phased.

### Phase boundaries matter

When working on a phase:

- only do the work defined for that phase
- do not prematurely implement future phases
- do not make unrelated cleanups outside the scoped work
- do not silently rewrite major subsystems

### Keep each phase reviewable

- Changes should be easy to inspect in PR form.
- Avoid giant mixed refactors.

---

## UI architecture expectations

The target architecture should separate concerns clearly:

- UI model/state
- encoder input handling
- rendering backend
- parameter formatting / display conversion
- page-table metadata and mapping

Do not keep everything monolithic if a clearer structure is possible.

---

## Rendering expectations

The target display is embedded and performance-sensitive.

Preferred behavior:

- redraw only when necessary
- use section-based or dirty-region updates
- avoid full-screen redraw on every loop
- avoid animations unless extremely lightweight
- avoid heap-heavy approaches
- keep UI work off the audio-critical path

---

## Input expectations

The old panel model depended on multiple buttons and analog inputs. The new UI must support:

- encoder rotation
- short click
- long click

Expected interaction concepts:

- navigation state
- edit state
- safe confirmation state for destructive actions

---

## Dangerous actions

The following types of operations must never become one-accident actions:

- write program
- write panel params
- read program
- read/init panel params
- panic
- sequencer randomize actions

They should require a focused, explicit confirmation flow.

---

## Prompt-driven phase files

Phased prompts live under:

- `codex/prompts/phase-1.md`
- `codex/prompts/phase-2.md`
- `codex/prompts/phase-3.md`
- `codex/prompts/phase-4.md`
- `codex/prompts/phase-5.md`

When asked to work on a phase, follow the corresponding prompt strictly.

---

## Markers / workflow expectations

Phase completion may be tracked using marker files under:

- `docs/migration-status/`

Do not mark a phase complete unless the scoped work is actually done and the repository is left in a coherent state.

---

## Coding style guidance

- Prefer explicit state machines over tangled booleans
- Prefer small helper functions over large mixed blocks
- Prefer descriptive naming over clever abbreviations
- Keep logic traceable to the original parameter/page model
- Keep new hardware-specific constants isolated and obvious
- Use static or lightweight memory patterns appropriate for embedded use

---

## What to avoid

- rewriting the synth engine to fit the UI
- mixing render logic with synth logic
- mixing physical input handling with display drawing
- hiding major behavioral changes in “cleanup”
- speculative abstractions that add complexity without benefit
- introducing undocumented assumptions as if they were facts

---

## Validation expectations

When completing a phase, verify as appropriate:

- navigation still works
- values are editable
- destructive actions are guarded
- display remains readable
- no obvious regressions in audio behavior
- code remains maintainable and scoped to the phase

---

## Final note

This repository should evolve through careful, bounded steps. Favor correctness, stability, and reviewability over speed.
