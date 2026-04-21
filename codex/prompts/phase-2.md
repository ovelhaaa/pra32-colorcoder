Phase 2 — Encoder input model and UI state machine

You are working in the repository "ovelhaaa/pra32-colorcoder".

This phase builds on the architecture cleanup from phase 1.

Do not implement the final ST7789 renderer yet.
Do not optimize redraw behavior yet beyond what is minimally necessary for clean structure.
Do not perform broad visual redesign in this phase.

Primary goal

Implement the new interaction model based on:

- one rotary encoder
- short click
- long click

and create an explicit UI state machine that replaces the old assumption of:

- many dedicated buttons
- 3 simultaneously editable analog controls

Hard constraints

- Preserve synth/audio behavior
- Preserve page-table-driven parameter access
- Preserve parameter semantics and ranges
- Preserve sequencer behavior
- Do not implement unsafe one-click destructive actions
- Do not entangle input logic with rendering logic

Required interaction model

Implement explicit states, with clear code structure.

Recommended state categories:

- group navigation
- page navigation
- item navigation
- item edit
- action confirmation

Equivalent naming is acceptable if clear.

Required behavior

Navigation

- encoder rotation moves selection/focus within the current state
- short click enters the next level or starts editing
- long click moves up/back or exits the current mode safely

Editing

- when editing a normal parameter, encoder rotation changes the value
- short click commits and exits edit mode
- long click cancels or exits edit mode safely

Dangerous/special actions

At minimum, protect these operations with a confirmation flow:

- write program
- write panel params
- read program
- read/init panel params
- panic
- seq randomize pitch
- seq randomize velocity

Implement a clear confirm state:

- focus action
- short click enters confirmation
- encoder selects confirm/cancel
- short click executes if confirm
- long click cancels

Required code outcomes

1. Encoder input abstraction

Implement a clean input abstraction that yields events such as:

- rotation delta
- short click
- long click

If exact hardware pins are not yet known, keep them as isolated configuration placeholders.

2. Focusable item model

Pages should no longer be treated purely as “A/B/C analog targets”.
Instead, the page definition should be adapted into focusable items such as:

- parameter A if valid
- parameter B if valid
- parameter C if valid
- action item(s) where applicable

3. Safe action model

Introduce a way to distinguish:

- ordinary editable parameters
- internal/local values if any
- actions requiring confirmation

4. Render independence

The new navigation/edit state machine must not depend on a particular display backend.

Non-goals for this phase

- no final ST7789 renderer
- no final widescreen layout
- no heavy visual polish
- no dirty-region optimization phase yet
- no unrelated synth changes

Acceptance criteria

- the codebase contains an explicit encoder-based UI state machine
- the old assumptions about dedicated prev/next/play/prog/shift interaction are no longer required for the new path
- parameters remain reachable through the page model
- dangerous actions require confirmation
- the code is ready for a later renderer to display focused items and edit state clearly

Deliverables

- encoder input/state-machine implementation
- clear summary of UI states and transitions
- notes on any hardware-specific constants still left configurable
