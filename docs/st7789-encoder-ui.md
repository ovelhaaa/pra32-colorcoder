# ST7789 + Encoder UI Architecture (Finalized in Phase 5)

This document describes the migrated control-panel UI used by the PRA32-U2/P path when single-encoder input and ST7789 output are enabled.

## Scope and safety constraints

The UI migration keeps existing synth/MIDI semantics intact:

- synth engine behavior is unchanged
- CC/controller numbering and value ranges are unchanged
- page/group/item definitions remain page-table-driven (`pra32-u2-control-panel-page-table.h`)
- destructive actions keep explicit confirmation and are never one-click operations

## Module layout

UI logic is split into small modules:

- `pra32-u2-ui-model.*`
  - Shared control-panel UI state (current group/page indices, panel/seq state values used by panel features).
- `pra32-u2-ui-input-encoder.*`
  - Encoder quadrature + switch polling.
  - Emits `rotation_delta`, `short_click`, and `long_click` events.
- `pra32-u2-ui-state-machine.*`
  - Navigation/edit/confirm state machine.
  - Rebuilds focusable items from page-table entries each event.
  - Applies value changes through callbacks so synth logic stays separate.
- `pra32-u2-ui-format.*`
  - Value formatting and pitch/scale presentation text.
- `pra32-u2-ui-render-st7789.*`
  - ST7789 renderer with dirty-section redraw (header/main/footer/overlay).
- `pra32-u2-control-panel.h`
  - Integration layer that connects page-table, synth callbacks, input polling, and rendering.

## Encoder interaction model

State progression:

1. **Group navigation**
   - Turn: cycle A/B/C/D groups.
   - Click: enter page navigation.
2. **Page navigation**
   - Turn: cycle pages in current group.
   - Click: enter item navigation.
   - Long click: back to group navigation.
3. **Item navigation**
   - Turn: move focused item among visible A/B/C entries.
   - Click on parameter: enter edit mode.
   - Click on dangerous action: enter confirm mode.
   - Long click: back to page navigation.
4. **Item edit**
   - Turn: adjust target value in 0..127 range.
   - Click: keep edited value and return to item navigation.
   - Long click: cancel edit (restore value captured on edit entry).
5. **Action confirm**
   - Turn: toggle `No` / `Yes`.
   - Click: execute only when `Yes` is selected; otherwise return.
   - Long click: cancel and return.

## Dangerous action confirmation

The following targets are guarded by confirmation state:

- `WR_PROGRAM_0..WR_PROGRAM_15`
- `RD_PROGRAM_0..RD_PROGRAM_15`
- `WR_PANEL_PRMS`
- `RD_PANEL_PRMS`
- `IN_PANEL_PRMS`
- `PANIC_OP`
- `SEQ_RAND_PITCH`
- `SEQ_RAND_VELO`

## Display and redraw behavior

The ST7789 renderer uses a frame diff strategy:

- skip all drawing when the frame is identical
- update only dirty sections
  - header (group/page/state/status)
  - main cards (only changed cards)
  - footer help strip (state-dependent controls)
  - confirm overlay strip (confirm state visibility)

This keeps redraw conservative and avoids full-screen refresh on every loop.

## Hardware configuration points

The default pin and geometry configuration is in `Digital-Synth-PRA32-U2.ino`.

Encoder:

- `PRA32_U2_ENCODER_PIN_A`
- `PRA32_U2_ENCODER_PIN_B`
- `PRA32_U2_ENCODER_SWITCH_PIN`
- `PRA32_U2_ENCODER_ACTIVE_LEVEL`
- `PRA32_U2_ENCODER_PIN_MODE`
- `PRA32_U2_ENCODER_DEBOUNCE_TICKS`
- `PRA32_U2_ENCODER_LONG_PRESS_TICKS`

ST7789:

- `PRA32_U2_ST7789_WIDTH` / `PRA32_U2_ST7789_HEIGHT`
- `PRA32_U2_ST7789_ROTATION`
- `PRA32_U2_ST7789_PIN_SCK`
- `PRA32_U2_ST7789_PIN_MOSI`
- `PRA32_U2_ST7789_PIN_CS`
- `PRA32_U2_ST7789_PIN_DC`
- `PRA32_U2_ST7789_PIN_RST`

## Remaining hardware setup required from users

Before flashing on real hardware, users still need to:

1. Uncomment `#define PRA32_U2_USE_CONTROL_PANEL` in `Digital-Synth-PRA32-U2.ino`.
2. Verify encoder pin wiring and active level against the board.
3. Verify ST7789 SPI/pin mapping and rotation.
4. Build and validate interaction on target hardware (especially long-click timing preference).
