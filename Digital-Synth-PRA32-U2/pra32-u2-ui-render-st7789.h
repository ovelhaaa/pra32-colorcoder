#pragma once

#include "pra32-u2-common.h"
#include "pra32-u2-ui-state-machine.h"

struct PRA32_U2_UI_RenderItem {
  bool visible;
  uint8_t source_index;
  uint8_t target;
  uint8_t value;
  PRA32_U2_UI_FocusItemType type;
  bool focused;
  bool has_value_text;
  char short_label[11];
  char value_text[5];
};

struct PRA32_U2_UI_RenderFrame {
  uint8_t page_group;
  uint8_t page_index;
  uint8_t page_count;
  PRA32_U2_UI_State state;
  bool confirm_selected;
  char page_name[22];
  char mode_text[4];
  char status_text[12];
  PRA32_U2_UI_RenderItem items[3];
};

void PRA32_U2_UI_RenderST7789_setup();
void PRA32_U2_UI_RenderST7789_draw(const PRA32_U2_UI_RenderFrame& frame);
