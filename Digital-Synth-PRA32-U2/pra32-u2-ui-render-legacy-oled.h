#pragma once

#include "pra32-u2-common.h"
#include "hardware/i2c.h"

#include <cstring>

static INLINE void PRA32_U2_ControlPanel_set_draw_position(uint8_t x, uint8_t y) {
#if defined(PRA32_U2_USE_CONTROL_PANEL)
  uint8_t commands[] = {0x00,  static_cast<uint8_t>(0xB0 + y),
                               static_cast<uint8_t>(0x10 + ((x * 6) >> 4)),
                               static_cast<uint8_t>(0x00 + ((x * 6) & 0x0F))};
  i2c_write_blocking(PRA32_U2_OLED_DISPLAY_I2C, PRA32_U2_OLED_DISPLAY_I2C_ADDRESS, commands, sizeof(commands), false);
#endif
}

static INLINE void PRA32_U2_ControlPanel_draw_character(uint8_t c) {
#if defined(PRA32_U2_USE_CONTROL_PANEL)
  uint8_t data[] = {0x40,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  std::memcpy(&data[1], g_control_panel_font_table[c], 6);
  i2c_write_blocking(PRA32_U2_OLED_DISPLAY_I2C, PRA32_U2_OLED_DISPLAY_I2C_ADDRESS, data, sizeof(data), false);
#endif
}
