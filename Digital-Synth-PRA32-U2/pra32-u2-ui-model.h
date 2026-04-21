#pragma once

#include "pra32-u2-common.h"
#include "pra32-u2-control-panel-page-table.h"

// Display-independent UI/panel state model extracted from the legacy monolithic control panel.
extern volatile uint32_t s_current_synth;
extern volatile uint32_t s_current_program[PRA32_U2_NUMBER_OF_SYNTHS];
extern volatile uint32_t s_current_page_group;
extern volatile uint32_t s_current_page_index[NUMBER_OF_PAGE_GROUPS];

extern volatile int32_t  s_adc_current_value[3];
extern volatile uint8_t  s_adc_control_value[3];
extern volatile uint8_t  s_adc_control_target[3];
extern volatile boolean  s_adc_control_catched[3];

extern uint8_t  s_play_mode;
extern int8_t   s_panel_transpose;
extern int8_t   s_panel_pit_ofst;
extern int8_t   s_seq_pit_ofst;
extern uint8_t  s_seq_step_clock_candidate;
extern uint8_t  s_seq_step_clock;
extern uint8_t  s_seq_gate_time;
extern int32_t  s_seq_last_step;
extern uint8_t  s_seq_mode;
extern int8_t   s_seq_mode_dir;
extern uint8_t  s_seq_on_steps;
extern uint8_t  s_seq_act_steps;

extern uint32_t s_index_scale;

extern int32_t  s_seq_step;
extern uint32_t s_seq_sub_step;
extern uint32_t s_seq_count;
extern uint32_t s_seq_count_increment;
extern bool     s_seq_clock_src_external;

extern volatile uint8_t  s_panel_play_note_pitch;
extern volatile uint8_t  s_panel_play_note_velocity;
extern volatile bool     s_panel_play_note_gate;
extern volatile bool     s_panel_play_note_trigger;

extern uint8_t  s_panel_playing_note_pitch;

extern volatile int32_t s_display_draw_counter;
