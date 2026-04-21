#include "pra32-u2-ui-input-legacy.h"

void PRA32_U2_UI_InputLegacy_update_analog_inputs(uint32_t loop_counter, volatile int32_t adc_current_value[3]) {
#if defined(PRA32_U2_USE_CONTROL_PANEL)
#if defined(PRA32_U2_USE_CONTROL_PANEL_ANALOG_INPUT)
  static int32_t s_adc_total_value = 0;
  switch (loop_counter & 0x3F) {
  case 0x10:
    adc_select_input(0);
    s_adc_total_value  = PRA32_U2_ANALOG_INPUT_CORRECTION;
    s_adc_total_value += adc_read() + adc_read() + adc_read() + adc_read();
    break;
  case 0x14:
    s_adc_total_value += adc_read() + adc_read() + adc_read() + adc_read();
    break;
  case 0x18:
    s_adc_total_value += adc_read() + adc_read() + adc_read() + adc_read();
    break;
  case 0x1C:
    s_adc_total_value += adc_read() + adc_read() + adc_read() + adc_read();
    if        (adc_current_value[0]                                   >= s_adc_total_value + PRA32_U2_ANALOG_INPUT_THRESHOLD) {
      adc_current_value[0] = s_adc_total_value;
    } else if (adc_current_value[0] + PRA32_U2_ANALOG_INPUT_THRESHOLD <= s_adc_total_value                                 ) {
      adc_current_value[0] = s_adc_total_value;
    }
    break;
  case 0x20:
    adc_select_input(1);
    s_adc_total_value  = PRA32_U2_ANALOG_INPUT_CORRECTION;
    s_adc_total_value += adc_read() + adc_read() + adc_read() + adc_read();
    break;
  case 0x24:
    s_adc_total_value += adc_read() + adc_read() + adc_read() + adc_read();
    break;
  case 0x28:
    s_adc_total_value += adc_read() + adc_read() + adc_read() + adc_read();
    break;
  case 0x2C:
    s_adc_total_value += adc_read() + adc_read() + adc_read() + adc_read();
    if        (adc_current_value[1]                                   >= s_adc_total_value + PRA32_U2_ANALOG_INPUT_THRESHOLD) {
      adc_current_value[1] = s_adc_total_value;
    } else if (adc_current_value[1] + PRA32_U2_ANALOG_INPUT_THRESHOLD <= s_adc_total_value                                 ) {
      adc_current_value[1] = s_adc_total_value;
    }
    break;
  case 0x30:
    adc_select_input(2);
    s_adc_total_value  = PRA32_U2_ANALOG_INPUT_CORRECTION;
    s_adc_total_value += adc_read() + adc_read() + adc_read() + adc_read();
    break;
  case 0x34:
    s_adc_total_value += adc_read() + adc_read() + adc_read() + adc_read();
    break;
  case 0x38:
    s_adc_total_value += adc_read() + adc_read() + adc_read() + adc_read();
    break;
  case 0x3C:
    s_adc_total_value += adc_read() + adc_read() + adc_read() + adc_read();
    if        (adc_current_value[2]                                   >= s_adc_total_value + PRA32_U2_ANALOG_INPUT_THRESHOLD) {
      adc_current_value[2] = s_adc_total_value;
    } else if (adc_current_value[2] + PRA32_U2_ANALOG_INPUT_THRESHOLD <= s_adc_total_value                                 ) {
      adc_current_value[2] = s_adc_total_value;
    }
    break;
  }
#else
  static_cast<void>(loop_counter);
  static_cast<void>(adc_current_value);
#endif
#else
  static_cast<void>(loop_counter);
  static_cast<void>(adc_current_value);
#endif
}
