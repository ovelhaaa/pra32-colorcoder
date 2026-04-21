Phase 3 — ST7789 renderer and widescreen UI layout

You are working in the repository "ovelhaaa/pra32-colorcoder".

This phase builds on the architecture refactor and encoder state machine.

Do not perform final redraw/performance hardening in this phase beyond reasonable hygiene.
Do not rewrite synth logic.
Do not loosen the safety model for actions.

Primary goal

Implement the first working color UI backend for:

- ST7789
- 284x76
- landscape / widescreen layout

The UI should be clearly readable, encoder-friendly, and structurally aligned with the new state machine.

Hard constraints

- preserve synth/audio behavior
- keep rendering separate from synth logic
- keep rendering separate from input logic
- preserve page-table-driven parameter mapping
- do not add heavy graphics or animations
- do not force full-screen refresh on every loop by design if it can be avoided

Required layout model

The old 128x64 text-grid UI must not simply be stretched.
Design a new layout appropriate for 284x76 widescreen.

Required regions

Header

Must show:

- current group/page indicator
- page name
- current mode where relevant ("Nrm", "Seq", etc.)
- program and/or running status as space allows

Main content

Display the page’s focusable items as horizontally arranged cards or equivalent compact visual modules.

Each visible item should be able to show:

- short label
- current value
- translated musical/enum text if relevant
- clear focus highlight
- optional compact bar/indicator for continuous values

Footer

Must show concise context help for encoder actions such as:

- turn
- click
- hold

Equivalent wording is acceptable.

Color requirements

Use a coherent color identity by group.

Recommended:

- Group A: blue/cyan family
- Group B: green family
- Group C: orange family
- Group D: purple/magenta family

Focused items should have stronger contrast.
Dangerous actions should be visually distinct.

Text requirements

If current labels are too long, introduce short display labels.
Do not destroy the original page-table meaning.
Keep traceability from displayed item to actual parameter target.

Rendering requirements

- implement a working ST7789 rendering backend
- keep drawing code maintainable
- avoid fancy effects
- avoid unnecessary overdraw
- prefer simple primitives, text, and compact cards

Non-goals for this phase

- no final dirty-region performance hardening
- no speculative animation system
- no unrelated cleanup outside renderer/UI display integration
- no synth engine changes

Acceptance criteria

- there is a working ST7789 renderer path
- the new layout is readable on 284x76 landscape
- focused item, edit mode, and confirm mode are visually understandable
- values and musical text are presented clearly
- the display backend is clearly separated from UI state and synth logic

Deliverables

- ST7789 renderer implementation
- widescreen layout implementation
- concise explanation of layout decisions
- note of any still-open hardware configuration items
