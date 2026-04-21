Phase 4 — Redraw optimization, dirty regions, and performance hardening

You are working in the repository "ovelhaaa/pra32-colorcoder".

This phase builds on the new UI model, encoder interaction, and ST7789 rendering backend.

This phase is primarily about efficiency and embedded robustness.

Primary goal

Reduce rendering overhead and operational risk by implementing efficient redraw behavior and hardening the UI path so it is more suitable for embedded real-time use.

Hard constraints

- audio behavior must remain stable
- do not move heavy UI work into the audio-critical path
- do not add flashy animations
- do not add complexity that makes the UI harder to maintain
- do not change parameter semantics

Required work

1. Dirty-region or section-based redraw

Implement a redraw strategy that updates only the parts of the display that changed.

At minimum, separate redraw responsibility for:

- header
- item cards / main content
- footer/help
- confirm/overlay region

2. Avoid constant full redraw

Ensure the default UI loop does not repaint the entire screen on every iteration.

3. Minimize unnecessary redraw triggers

Redraw only when relevant state changes, such as:

- focus changed
- edit value changed
- page changed
- group changed
- mode changed
- status/help text changed
- confirmation overlay entered/exited

4. Embedded robustness

Reduce avoidable runtime costs such as:

- repeated full recomputation where not needed
- unnecessary string churn
- unnecessary allocations
- excessive SPI traffic

5. Code clarity

The redraw system must remain understandable.
Do not create an over-engineered retained-mode graphics framework.

Non-goals for this phase

- no broad visual redesign
- no synth engine changes
- no unrelated cleanup sprawl
- no decorative animation system

Acceptance criteria

- the UI updates only relevant regions or sections
- full redraws are avoided in normal operation
- the redraw logic is understandable and maintainable
- encoder movement and normal UI use do not obviously increase risk of audio glitches
- the implementation remains consistent with the phase architecture

Deliverables

- redraw optimization implementation
- concise summary of what triggers redraw and what does not
- any notes on remaining performance concerns or future tradeoffs
