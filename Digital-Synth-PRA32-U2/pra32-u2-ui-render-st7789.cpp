#include "pra32-u2-ui-render-st7789.h"

#if defined(PRA32_U2_USE_CONTROL_PANEL_ST7789_DISPLAY)

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

#include <cstring>

namespace {

Adafruit_ST7789 g_st7789(
  PRA32_U2_ST7789_PIN_CS,
  PRA32_U2_ST7789_PIN_DC,
  PRA32_U2_ST7789_PIN_RST
);

PRA32_U2_UI_RenderFrame g_prev_frame = {};
bool g_prev_frame_valid = false;

const uint16_t COLOR_BLACK         = ST77XX_BLACK;
const uint16_t COLOR_WHITE         = ST77XX_WHITE;
const uint16_t COLOR_DANGER        = ST77XX_RED;
const uint16_t COLOR_HEADER_TEXT   = ST77XX_WHITE;
const uint16_t COLOR_FOOTER_TEXT   = ST77XX_WHITE;
const uint16_t COLOR_CARD_TEXT     = ST77XX_WHITE;
const uint16_t COLOR_FOCUS_TEXT    = ST77XX_BLACK;
const uint16_t COLOR_HELP_BG       = ST77XX_BLACK;
const uint16_t COLOR_CARD_BG_NORMAL  = 0x18E3;
const uint16_t COLOR_CARD_BG_ACTION  = 0x3000;

const int DISPLAY_WIDTH   = PRA32_U2_ST7789_WIDTH;
const int HEADER_Y        = 0;
const int HEADER_HEIGHT   = 14;
const int MAIN_Y          = 18;
const int CARD_WIDTH      = 90;
const int CARD_HEIGHT     = 42;
const int CARD_GAP_X      = 4;
const int FOOTER_Y        = 64;
const int FOOTER_HEIGHT   = 12;

uint16_t group_background(uint8_t group) {
  switch (group) {
  case 0: return 0x03FF;  // cyan
  case 1: return 0x07E0;  // green
  case 2: return 0xFD20;  // orange
  case 3: return 0xD81F;  // magenta
  default: return ST77XX_BLUE;
  }
}

bool same_item(const PRA32_U2_UI_RenderItem& a, const PRA32_U2_UI_RenderItem& b) {
  return a.visible == b.visible &&
         a.source_index == b.source_index &&
         a.target == b.target &&
         a.value == b.value &&
         a.type == b.type &&
         a.focused == b.focused &&
         a.has_value_text == b.has_value_text &&
         std::strncmp(a.short_label, b.short_label, sizeof(a.short_label)) == 0 &&
         std::strncmp(a.value_text, b.value_text, sizeof(a.value_text)) == 0;
}

bool same_frame(const PRA32_U2_UI_RenderFrame& a, const PRA32_U2_UI_RenderFrame& b) {
  if (a.page_group != b.page_group ||
      a.page_index != b.page_index ||
      a.page_count != b.page_count ||
      a.state != b.state ||
      a.confirm_selected != b.confirm_selected ||
      std::strncmp(a.page_name, b.page_name, sizeof(a.page_name)) != 0 ||
      std::strncmp(a.mode_text, b.mode_text, sizeof(a.mode_text)) != 0 ||
      std::strncmp(a.status_text, b.status_text, sizeof(a.status_text)) != 0) {
    return false;
  }

  for (uint8_t i = 0; i < 3; ++i) {
    if (!same_item(a.items[i], b.items[i])) {
      return false;
    }
  }

  return true;
}

const char* state_short_text(PRA32_U2_UI_State state) {
  switch (state) {
  case PRA32_U2_UI_State_GroupNavigation: return "Grp";
  case PRA32_U2_UI_State_PageNavigation : return "Pag";
  case PRA32_U2_UI_State_ItemNavigation : return "Nav";
  case PRA32_U2_UI_State_ItemEdit       : return "Edt";
  case PRA32_U2_UI_State_ActionConfirm  : return "Cfm";
  default                               : return "UI ";
  }
}

void draw_footer_help(PRA32_U2_UI_State state, bool confirm_selected) {
  g_st7789.fillRect(0, FOOTER_Y, DISPLAY_WIDTH, FOOTER_HEIGHT, COLOR_HELP_BG);
  g_st7789.setTextSize(1);
  g_st7789.setTextColor(COLOR_FOOTER_TEXT);
  g_st7789.setCursor(2, 66);

  if (state == PRA32_U2_UI_State_ItemEdit) {
    g_st7789.print("Turn:Value  Click:Done  Hold:Cancel");
  } else if (state == PRA32_U2_UI_State_ActionConfirm) {
    g_st7789.print("Turn:No/Yes  Click:");
    g_st7789.print(confirm_selected ? "Exec" : "Back");
    g_st7789.print("  Hold:Back");
  } else {
    g_st7789.print("Turn:Move  Click:Select  Hold:Back");
  }
}

bool same_header(const PRA32_U2_UI_RenderFrame& a, const PRA32_U2_UI_RenderFrame& b) {
  return a.page_group == b.page_group &&
         a.page_index == b.page_index &&
         a.page_count == b.page_count &&
         a.state == b.state &&
         std::strncmp(a.page_name, b.page_name, sizeof(a.page_name)) == 0 &&
         std::strncmp(a.mode_text, b.mode_text, sizeof(a.mode_text)) == 0 &&
         std::strncmp(a.status_text, b.status_text, sizeof(a.status_text)) == 0;
}

}  // namespace

void PRA32_U2_UI_RenderST7789_setup() {
  SPI.begin(PRA32_U2_ST7789_PIN_SCK, -1, PRA32_U2_ST7789_PIN_MOSI, PRA32_U2_ST7789_PIN_CS);
  g_st7789.init(PRA32_U2_ST7789_WIDTH, PRA32_U2_ST7789_HEIGHT);
  g_st7789.setRotation(PRA32_U2_ST7789_ROTATION);
  g_st7789.fillScreen(COLOR_BLACK);
  g_prev_frame_valid = false;
}

void PRA32_U2_UI_RenderST7789_draw(const PRA32_U2_UI_RenderFrame& frame) {
  if (g_prev_frame_valid && same_frame(g_prev_frame, frame)) {
    return;
  }

  const bool redraw_header = !g_prev_frame_valid || !same_header(g_prev_frame, frame);
  const bool redraw_footer = !g_prev_frame_valid ||
                             (g_prev_frame.state != frame.state) ||
                             (g_prev_frame.confirm_selected != frame.confirm_selected);

  const uint16_t group_color = group_background(frame.page_group);
  if (redraw_header) {
    g_st7789.fillRect(0, HEADER_Y, DISPLAY_WIDTH, HEADER_HEIGHT, group_color);
    g_st7789.setTextSize(1);
    g_st7789.setTextColor(COLOR_HEADER_TEXT);

    g_st7789.setCursor(2, 3);
    g_st7789.print(static_cast<char>('A' + frame.page_group));
    g_st7789.print("-");
    g_st7789.print(frame.page_index);
    g_st7789.print("/");
    g_st7789.print(frame.page_count ? frame.page_count - 1 : 0);

    g_st7789.setCursor(44, 3);
    g_st7789.print(frame.page_name);

    g_st7789.setCursor(198, 3);
    g_st7789.print(frame.mode_text);
    g_st7789.print(" ");
    g_st7789.print(state_short_text(frame.state));

    g_st7789.setCursor(240, 3);
    g_st7789.print(frame.status_text);
  }

  for (uint8_t index = 0; index < 3; ++index) {
    const PRA32_U2_UI_RenderItem& item = frame.items[index];
    bool redraw_card = !g_prev_frame_valid || !same_item(item, g_prev_frame.items[index]);
    if (!redraw_card) {
      continue;
    }

    const int x = CARD_GAP_X + (index * (CARD_WIDTH + CARD_GAP_X));
    const int y = MAIN_Y;
    const int w = CARD_WIDTH;
    const int h = CARD_HEIGHT;

    g_st7789.fillRect(x, y, w, h, COLOR_BLACK);

    if (!item.visible) {
      continue;
    }

    uint16_t card_bg = COLOR_CARD_BG_NORMAL;
    uint16_t border_color = group_color;

    if (item.type == PRA32_U2_UI_FocusItemType_Action) {
      card_bg = COLOR_CARD_BG_ACTION;
      border_color = COLOR_DANGER;
    }

    if (item.focused) {
      card_bg = (item.type == PRA32_U2_UI_FocusItemType_Action) ? COLOR_DANGER : COLOR_WHITE;
      border_color = COLOR_WHITE;
    }

    g_st7789.fillRect(x, y, w, h, card_bg);
    g_st7789.drawRect(x, y, w, h, border_color);

    g_st7789.setTextColor(item.focused ? COLOR_FOCUS_TEXT : COLOR_CARD_TEXT);
    g_st7789.setCursor(x + 4, y + 4);
    g_st7789.print(static_cast<char>('A' + item.source_index));
    g_st7789.print(":");
    g_st7789.print(item.short_label);

    g_st7789.setCursor(x + 4, y + 16);
    if (item.has_value_text) {
      g_st7789.print(item.value_text);
      g_st7789.print(" (");
      g_st7789.print(item.value);
      g_st7789.print(")");
    } else {
      g_st7789.print(item.value);
    }

    if (item.type == PRA32_U2_UI_FocusItemType_Parameter) {
      const int bar_x = x + 4;
      const int bar_y = y + 32;
      const int bar_w = w - 8;
      const int fill_w = (bar_w * item.value) / 127;
      g_st7789.drawRect(bar_x, bar_y, bar_w, 6, item.focused ? COLOR_FOCUS_TEXT : border_color);
      g_st7789.fillRect(bar_x + 1, bar_y + 1, fill_w, 4, item.focused ? COLOR_FOCUS_TEXT : border_color);
    } else if (frame.state == PRA32_U2_UI_State_ActionConfirm && item.focused) {
      g_st7789.setCursor(x + 4, y + 31);
      g_st7789.print(frame.confirm_selected ? "[YES] no " : " yes [NO]");
    }
  }

  if (redraw_footer) {
    draw_footer_help(frame.state, frame.confirm_selected);
  }

  g_prev_frame = frame;
  g_prev_frame_valid = true;
}

#else

void PRA32_U2_UI_RenderST7789_setup() {
}

void PRA32_U2_UI_RenderST7789_draw(const PRA32_U2_UI_RenderFrame& frame) {
  static_cast<void>(frame);
}

#endif  // defined(PRA32_U2_USE_CONTROL_PANEL_ST7789_DISPLAY)
