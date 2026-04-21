#pragma once

#include "pra32-u2-common.h"

uint8_t PRA32_U2_ControlPanel_get_index_scale();
uint8_t PRA32_U2_ControlPanel_calc_scaled_pitch(uint32_t index_scale, uint8_t pitch, int panel_pit_ofst, int seq_pit_ofst);
uint8_t PRA32_U2_ControlPanel_calc_transposed_pitch(uint8_t pitch, int32_t transpose);
void PRA32_U2_ControlPanel_calc_value_display_pitch(uint8_t pitch, char value_display_text[5]);
uint32_t PRA32_U2_ControlPanel_calc_bpm(uint8_t tempo_control_value);
boolean PRA32_U2_ControlPanel_calc_value_display(uint8_t control_target, uint8_t controller_value, char value_display_text[5]);
