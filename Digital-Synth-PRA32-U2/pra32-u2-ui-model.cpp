#include "pra32-u2-ui-model.h"

volatile uint32_t s_current_synth = 0;
volatile uint32_t s_current_program[PRA32_U2_NUMBER_OF_SYNTHS] = {};
volatile uint32_t s_current_page_group = PAGE_GROUP_DEFAULT;
volatile uint32_t s_current_page_index[NUMBER_OF_PAGE_GROUPS] = {
  PAGE_INDEX_DEFAULT_A,
  PAGE_INDEX_DEFAULT_B,
  PAGE_INDEX_DEFAULT_C,
  PAGE_INDEX_DEFAULT_D,
};

volatile int32_t  s_adc_current_value[3] = {};
volatile uint8_t  s_adc_control_value[3] = {};
volatile uint8_t  s_adc_control_target[3] = { 0xFF, 0xFF, 0xFF };
volatile bool     s_adc_control_catched[3] = {};

uint8_t  s_play_mode                = 0;
int8_t   s_panel_transpose          = 0;
int8_t   s_panel_pit_ofst           = 0;
int8_t   s_seq_pit_ofst             = 0;
uint8_t  s_seq_step_clock_candidate = 12;
uint8_t  s_seq_step_clock           = 12;
uint8_t  s_seq_gate_time            = 6;
int32_t  s_seq_last_step            = 7;
uint8_t  s_seq_mode                 = 0;
int8_t   s_seq_mode_dir             = +1;
uint8_t  s_seq_on_steps             = 127;
uint8_t  s_seq_act_steps            = 127;

uint32_t s_index_scale              = 0;

int32_t  s_seq_step               = 31;
uint32_t s_seq_sub_step           = 23;
uint32_t s_seq_count              = 0;
uint32_t s_seq_count_increment    = 0;
bool     s_seq_clock_src_external = false;

volatile uint8_t  s_panel_play_note_pitch    = 60;
volatile uint8_t  s_panel_play_note_velocity = 127;
volatile bool     s_panel_play_note_gate     = false;
volatile bool     s_panel_play_note_trigger  = false;

uint8_t  s_panel_playing_note_pitch = 0xFF;

volatile int32_t s_display_draw_counter = -1;
