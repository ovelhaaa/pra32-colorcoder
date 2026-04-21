Phase 1 — Architecture refactor of control panel / UI code

You are working in the repository "ovelhaaa/pra32-colorcoder".

This phase is focused on architecture refactor only.

Do not implement the final encoder UI yet.
Do not implement the final ST7789 renderer yet.
Do not redesign the visible UI yet.

Your purpose in this phase is to prepare the codebase for the later UI migration while preserving behavior.

Primary goal

Refactor the current control-panel code so that the responsibilities are separated cleanly enough for future phases to add:

- encoder-based input
- a UI state machine
- an ST7789 renderer
- a new widescreen layout
- redraw optimization

without forcing a giant risky rewrite later.

Existing context

The current implementation is centered around files such as:

- "Digital-Synth-PRA32-U2/Digital-Synth-PRA32-U2.ino"
- "Digital-Synth-PRA32-U2/pra32-u2-control-panel.h"
- "Digital-Synth-PRA32-U2/pra32-u2-control-panel-page-table.h"

The current control-panel code mixes:

- panel state
- page/group navigation state
- input processing
- analog input handling
- display text formatting
- OLED rendering
- some panel operational logic

This monolithic structure must be improved.

Hard constraints

- Preserve synth/audio behavior
- Preserve sequencer behavior
- Preserve MIDI/control semantics
- Preserve page-table-driven parameter mapping
- Do not change parameter meaning or ranges
- Do not break existing buildability if the legacy panel path is still used
- Do not introduce future-phase rendering logic prematurely

Required outcomes

1. Separate concerns

Refactor toward clearer separation of:

- UI/panel state model
- input handling
- formatting/display-value translation
- rendering backend
- page-table metadata usage

2. Keep legacy behavior working

The current panel behavior should remain functional after the refactor, even if still backed by the legacy hardware path.

3. Prepare stable interfaces

Create or introduce clean internal interfaces so later phases can plug in:

- encoder input backend
- ST7789 rendering backend
- focus/edit state machine

4. Minimize risk

Do not re-architect the synth engine.
Do not change hot-path audio behavior.
Do not do opportunistic rewrites outside UI/control-panel architecture.

Suggested file organization

You may introduce files such as:

- "pra32-u2-ui-model.*"
- "pra32-u2-ui-format.*"
- "pra32-u2-ui-render-legacy-oled.*"
- "pra32-u2-ui-input-legacy.*"

Equivalent naming is acceptable if the structure is clear.

Specific requirements

UI model

Create a cleaner place for:

- current page group
- current page index per group
- current program/synth selection if relevant
- play/seq mode-related UI state
- any display-independent panel state

Formatting layer

Extract reusable formatting logic such as:

- note/pitch display
- enum display text
- BPM display
- display text for special operations
- numeric conversions tied to current parameter semantics

This logic should not remain mixed into low-level rendering code.

Rendering abstraction

The existing OLED rendering logic should be isolated behind a more obvious boundary.
Do not yet implement the ST7789 renderer, but make room for it.

Input abstraction

The existing button/ADC handling should be isolated enough that a future encoder backend can replace it.

Non-goals for this phase

- no encoder implementation
- no ST7789 implementation
- no final widescreen layout
- no full UI redesign
- no dirty-region rendering system yet
- no GitHub workflow changes unless strictly needed for this phase’s files

Acceptance criteria

- the control-panel code is less monolithic than before
- behavior is preserved
- formatting logic is reusable and no longer buried in rendering-heavy code
- there is a clean place to add encoder state and ST7789 rendering in later phases
- the code is easier to review and reason about than before

Deliverables

- the architectural refactor in code
- a concise summary of the new internal module boundaries
- any notes about what remains intentionally deferred to later phases
