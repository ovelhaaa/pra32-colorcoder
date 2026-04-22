#include "pra32-u2-ui-input-encoder.h"

#ifndef PRA32_U2_ENCODER_PIN_A
#define PRA32_U2_ENCODER_PIN_A (14)
#endif

#ifndef PRA32_U2_ENCODER_PIN_B
#define PRA32_U2_ENCODER_PIN_B (15)
#endif

#ifndef PRA32_U2_ENCODER_SWITCH_PIN
#define PRA32_U2_ENCODER_SWITCH_PIN (21)
#endif

#ifndef PRA32_U2_ENCODER_ACTIVE_LEVEL
#define PRA32_U2_ENCODER_ACTIVE_LEVEL (LOW)
#endif

#ifndef PRA32_U2_ENCODER_PIN_MODE
#define PRA32_U2_ENCODER_PIN_MODE (INPUT_PULLUP)
#endif

#ifndef PRA32_U2_ENCODER_DEBOUNCE_TICKS
#define PRA32_U2_ENCODER_DEBOUNCE_TICKS (15)
#endif

#ifndef PRA32_U2_ENCODER_LONG_PRESS_TICKS
#define PRA32_U2_ENCODER_LONG_PRESS_TICKS (375)
#endif

void PRA32_U2_UI_EncoderInput_setup() {
#if defined(PRA32_U2_USE_CONTROL_PANEL) && defined(PRA32_U2_USE_CONTROL_PANEL_ENCODER_INPUT)
  pinMode(PRA32_U2_ENCODER_PIN_A, PRA32_U2_ENCODER_PIN_MODE);
  pinMode(PRA32_U2_ENCODER_PIN_B, PRA32_U2_ENCODER_PIN_MODE);
  pinMode(PRA32_U2_ENCODER_SWITCH_PIN, PRA32_U2_ENCODER_PIN_MODE);
#endif
}

PRA32_U2_UI_EncoderInputEvent PRA32_U2_UI_EncoderInput_poll() {
  PRA32_U2_UI_EncoderInputEvent event = {0, false, false};

#if defined(PRA32_U2_USE_CONTROL_PANEL) && defined(PRA32_U2_USE_CONTROL_PANEL_ENCODER_INPUT)
  static uint8_t prev_a = 0;
  static uint8_t prev_sw = 0;
  static uint32_t tick = 0;
  static uint32_t sw_changed_tick = 0;
  static bool long_sent = false;

  ++tick;

  uint8_t a = digitalRead(PRA32_U2_ENCODER_PIN_A);
  if (a != prev_a) {
    uint8_t b = digitalRead(PRA32_U2_ENCODER_PIN_B);
    event.rotation_delta = (a == b) ? +1 : -1;
    prev_a = a;
  }

  uint8_t sw = digitalRead(PRA32_U2_ENCODER_SWITCH_PIN) == PRA32_U2_ENCODER_ACTIVE_LEVEL;
  if (sw != prev_sw) {
    if ((tick - sw_changed_tick) >= PRA32_U2_ENCODER_DEBOUNCE_TICKS) {
      sw_changed_tick = tick;
      prev_sw = sw;
      if (!sw) {
        if (!long_sent) {
          event.short_click = true;
        }
        long_sent = false;
      }
    }
  } else if (sw && !long_sent) {
    if ((tick - sw_changed_tick) >= PRA32_U2_ENCODER_LONG_PRESS_TICKS) {
      event.long_click = true;
      long_sent = true;
    }
  }
#endif

  return event;
}
