#pragma once

#include "pra32-u2-common.h"
#include "pra32-u2-control-panel-page-table.h"

// Display-independent UI/panel state model extracted from the legacy monolithic control panel.
static volatile uint32_t s_current_synth = 0;
static volatile uint32_t s_current_program[PRA32_U2_NUMBER_OF_SYNTHS] = {};
static volatile uint32_t s_current_page_group   = PAGE_GROUP_DEFAULT;
static volatile uint32_t s_current_page_index[] = { PAGE_INDEX_DEFAULT_A, PAGE_INDEX_DEFAULT_B, PAGE_INDEX_DEFAULT_C, PAGE_INDEX_DEFAULT_D };

static volatile int32_t  s_adc_current_value[3];
static volatile uint8_t  s_adc_control_value[3];
static volatile uint8_t  s_adc_control_target[3] = { 0xFF, 0xFF, 0xFF };
static volatile boolean  s_adc_control_catched[3];

static uint8_t  s_play_mode                = 0;
static int8_t   s_panel_transpose          = 0;
static int8_t   s_panel_pit_ofst           = 0;
static int8_t   s_seq_pit_ofst             = 0;
static uint8_t  s_seq_step_clock_candidate = 12;
static uint8_t  s_seq_step_clock           = 12;
static uint8_t  s_seq_gate_time            = 6;
static int32_t  s_seq_last_step            = 7;
static uint8_t  s_seq_mode                 = 0;
static int8_t   s_seq_mode_dir             = +1;
static uint8_t  s_seq_on_steps             = 127;
static uint8_t  s_seq_act_steps            = 127;

static uint32_t s_index_scale              = 0;

static int32_t  s_seq_step               = 31;
static uint32_t s_seq_sub_step           = 23;
static uint32_t s_seq_count              = 0;
static uint32_t s_seq_count_increment    = 0;
static bool     s_seq_clock_src_external = false;

static volatile uint8_t  s_panel_play_note_pitch    = 60;
static volatile uint8_t  s_panel_play_note_velocity = 127;
static volatile bool     s_panel_play_note_gate     = false;
static volatile bool     s_panel_play_note_trigger  = false;

static uint8_t  s_panel_playing_note_pitch = 0xFF;

static volatile int32_t s_display_draw_counter = -1;
