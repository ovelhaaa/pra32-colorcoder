#pragma once

#include "pra32-u2-common.h"

struct PRA32_U2_UI_EncoderInputEvent {
  int8_t rotation_delta;
  bool short_click;
  bool long_click;
};

void PRA32_U2_UI_EncoderInput_setup();
PRA32_U2_UI_EncoderInputEvent PRA32_U2_UI_EncoderInput_poll();
