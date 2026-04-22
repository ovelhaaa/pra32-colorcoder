#pragma once

#include "pra32-u2-common.h"
#include "pra32-u2-control-panel-font-table.h"
#include "pra32-u2-control-panel-page-table.h"
#include "pra32-u2-ui-model.h"
#include "pra32-u2-ui-format.h"
#include "pra32-u2-ui-render-legacy-oled.h"
#include "pra32-u2-ui-render-st7789.h"
#include "pra32-u2-ui-input-legacy.h"
#include "pra32-u2-ui-input-encoder.h"
#include "pra32-u2-ui-state-machine.h"

#include "hardware/i2c.h"

#include <cstdio>
#include <cstring>


#if defined(PRA32_U2_USE_CONTROL_PANEL)
static          uint32_t s_prev_key_current_value;
static          uint32_t s_next_key_current_value;
static          uint32_t s_play_key_current_value;

static          uint32_t s_prog_minus_key_current_value;
static          uint32_t s_prog_plus_key_current_value;
#endif  // defined(PRA32_U2_USE_CONTROL_PANEL)


enum PlayingStatus {
  PlayingStatus_Stop = 0,
  PlayingStatus_Playing,
  PlayingStatus_Seq,
};

static          uint32_t s_playing_status = PlayingStatus_Stop;


static char s_display_buffer[8][21 + 1] = {
  "                     ",
  "                     ",
  "                     ",
  "                     ",
  "                     ",
  "                     ",
  "                     ",
  "                     ",
};


extern void handleNoteOn(byte channel, byte pitch, byte velocity);
extern void handleNoteOff(byte channel, byte pitch, byte velocity);
extern void handleControlChange(byte channel, byte number, byte value);
extern void handleProgramChange(byte channel, byte number);
extern uint8_t getCurrentControllerValue(byte channel, byte number);
extern void getRandUint8Rrray(byte channel, uint8_t array[8]);
extern void writeParametersToProgram(byte channel, byte number);

static INLINE uint8_t PRA32_U2_ControlPanel_get_target_value(uint8_t target);
static INLINE void PRA32_U2_ControlPanel_set_target_value(uint8_t target, uint8_t value);
static INLINE void PRA32_U2_ControlPanel_execute_action_target(uint8_t target);
static INLINE void PRA32_U2_ControlPanel_build_short_label(const char line0[11], const char line1[11], char out[11]);
static INLINE void PRA32_U2_ControlPanel_build_st7789_frame(PRA32_U2_UI_RenderFrame& frame);
static INLINE void PRA32_U2_ControlPanel_request_st7789_redraw();
static INLINE bool PRA32_U2_ControlPanel_st7789_target_is_visible(uint8_t target);


static INLINE void PRA32_U2_ControlPanel_update_page() {
  s_adc_control_catched[0] = false;
  s_adc_control_catched[1] = false;
  s_adc_control_catched[2] = false;

  PRA32_U2_ControlPanelPage current_page = g_control_panel_page_table[s_current_page_group][s_current_page_index[s_current_page_group]];

  if (s_play_mode == 1) {  // Seq Mode
    std::memcpy(current_page.control_target_c_name_line_0, "Seq       ", 10);
    std::memcpy(current_page.control_target_c_name_line_1, "Pitch Ofst", 10);
    current_page.control_target_c = SEQ_PIT_OFST   ;
  }

  std::memcpy(&s_display_buffer[1][ 0], current_page.page_name_line_0            , 10);
  std::memcpy(&s_display_buffer[2][ 0], current_page.page_name_line_1            , 10);

  std::memcpy(&s_display_buffer[5][ 0], current_page.control_target_a_name_line_0, 10);
  std::memcpy(&s_display_buffer[6][ 0], current_page.control_target_a_name_line_1, 10);
  s_adc_control_target[0]             = current_page.control_target_a;

  std::memcpy(&s_display_buffer[5][11], current_page.control_target_b_name_line_0, 10);
  std::memcpy(&s_display_buffer[6][11], current_page.control_target_b_name_line_1, 10);
  s_adc_control_target[1]             = current_page.control_target_b;

#if defined(PRA32_U2_KEY_INPUT_PLAY_KEY_PIN)
  std::memcpy(&s_display_buffer[1][11], current_page.control_target_c_name_line_0, 10);
  std::memcpy(&s_display_buffer[2][11], current_page.control_target_c_name_line_1, 10);
  s_adc_control_target[2]             = current_page.control_target_c;
#endif  // defined(PRA32_U2_KEY_INPUT_PLAY_KEY_PIN)

#if (PRA32_U2_NUMBER_OF_SYNTHS > 1)
  s_display_buffer[0][13] = '$';
  s_display_buffer[0][14] = '0' + s_current_synth;
#endif  // (PRA32_U2_NUMBER_OF_SYNTHS > 1)

#if defined(PRA32_U2_KEY_INPUT_PROG_MINUS_KEY_PIN) || defined(PRA32_U2_KEY_INPUT_PROG_PLUS_KEY_PIN)
  s_display_buffer[0][16] = '#';
  s_display_buffer[0][17] = '0' + s_current_program[s_current_synth];
#endif  // defined(PRA32_U2_KEY_INPUT_PROG_MINUS_KEY_PIN) || defined(PRA32_U2_KEY_INPUT_PROG_PLUS_KEY_PIN)

  s_display_draw_counter = -1;
  PRA32_U2_ControlPanel_request_st7789_redraw();
}

static INLINE uint8_t PRA32_U2_ControlPanel_adc_control_value_candidate(uint32_t adc_number) {
  volatile int32_t adc_control_value_candidate;

#if defined(PRA32_U2_USE_CONTROL_PANEL)

#if defined(PRA32_U2_ANALOG_INPUT_REVERSED)
  adc_control_value_candidate = (127 - (s_adc_current_value[adc_number] / PRA32_U2_ANALOG_INPUT_DENOMINATOR));
#else  // defined(PRA32_U2_ANALOG_INPUT_REVERSED)
  adc_control_value_candidate = (s_adc_current_value[adc_number] / PRA32_U2_ANALOG_INPUT_DENOMINATOR);
#endif  // defined(PRA32_U2_ANALOG_INPUT_REVERSED)

#endif  // defined(PRA32_U2_USE_CONTROL_PANEL)

  if (adc_control_value_candidate > 127) {
    adc_control_value_candidate = 127;
  }

  if (adc_control_value_candidate < 0) {
    adc_control_value_candidate = 0;
  }

  return adc_control_value_candidate;
}

static INLINE boolean PRA32_U2_ControlPanel_process_note_off_on() {
  if (s_panel_playing_note_pitch <= 127) {
    if (s_panel_play_note_gate == false) {
      handleNoteOff(((g_midi_ch + s_current_synth) & 0x0F) + 1, s_panel_playing_note_pitch, 64);
      s_panel_playing_note_pitch = 0xFF;
    } else {
      if (s_panel_play_note_trigger && (s_panel_play_note_pitch <= 127)) {
        s_panel_play_note_trigger = false;

        handleNoteOn(((g_midi_ch + s_current_synth) & 0x0F) + 1, s_panel_play_note_pitch, s_panel_play_note_velocity);
        handleNoteOff(((g_midi_ch + s_current_synth) & 0x0F) + 1, s_panel_playing_note_pitch, 64);
        s_panel_playing_note_pitch = s_panel_play_note_pitch;
      } else {
        s_panel_play_note_trigger = false;
      }
    }
  } else {
    if (s_panel_play_note_gate && (s_panel_play_note_pitch <= 127)) {
      s_panel_play_note_trigger = false;

      handleNoteOn(((g_midi_ch + s_current_synth) & 0x0F) + 1, s_panel_play_note_pitch, s_panel_play_note_velocity);
      s_panel_playing_note_pitch = s_panel_play_note_pitch;
    } else {
      s_panel_play_note_trigger = false;
    }
  }

  return false;
}

static INLINE void PRA32_U2_ControlPanel_update_pitch(bool progress_seq_step) {
  uint8_t new_pitch    = 60;
  uint8_t new_velocity = 127;

  if (s_play_mode == 0) {  // Normal Mode
    new_pitch         = g_synth.current_controller_value(PANEL_PLAY_PIT );
    new_velocity      = g_synth.current_controller_value(PANEL_PLAY_VELO);

    s_index_scale     = PRA32_U2_ControlPanel_get_index_scale();
    s_panel_pit_ofst  = g_synth.current_controller_value(PANEL_PIT_OFST ) - 64;
    s_panel_transpose = g_synth.current_controller_value(PANEL_TRANSPOSE) - 64;
    s_seq_pit_ofst    = 0;
  } else {  // Seq Mode
    new_pitch         = g_synth.current_controller_value(SEQ_PITCH_0      + (s_seq_step & 0x07));
    new_velocity      = g_synth.current_controller_value(SEQ_VELO_0       + (s_seq_step & 0x07));
    if (((s_seq_step & 0x07) != 0) && ((1 << ((s_seq_step & 0x07) - 1)) & s_seq_on_steps) == 0) {
      new_velocity = 0;
    }
  }

  new_pitch = PRA32_U2_ControlPanel_calc_scaled_pitch(s_index_scale, new_pitch, s_panel_pit_ofst, s_seq_pit_ofst);

  if (new_pitch == 0xFF) {
    s_panel_play_note_pitch = 0xFF;
    s_panel_play_note_gate  = false;
    return;
  }

  new_pitch = PRA32_U2_ControlPanel_calc_transposed_pitch(new_pitch, s_panel_transpose);

  s_panel_play_note_velocity = new_velocity;

  bool panel_play_note_number_changed = false;

  if (s_panel_play_note_pitch != new_pitch) {
    s_panel_play_note_pitch = new_pitch;
    panel_play_note_number_changed = true;
  }

  if (((s_playing_status == PlayingStatus_Playing) && panel_play_note_number_changed) ||
      ((s_playing_status == PlayingStatus_Seq) && progress_seq_step)) {
    s_panel_play_note_gate    = true;
    s_panel_play_note_trigger = true;
  }
}

static INLINE void PRA32_U2_ControlPanel_seq_clock() {
#if defined(PRA32_U2_USE_USB_MIDI) && !defined(PRA32_U2_DISABLE_USB_MIDI_TRANSMITTION)
  USB_MIDI.sendRealTime(midi::Clock);
#endif  // defined(PRA32_U2_USE_USB_MIDI) && !defined(PRA32_U2_DISABLE_USB_MIDI_TRANSMITTION)

#if defined(PRA32_U2_USE_UART_MIDI)
  UART_MIDI.sendRealTime(midi::Clock);
#endif  // defined(PRA32_U2_USE_UART_MIDI)

  if (s_playing_status != PlayingStatus_Seq) {
    return;
  }

  ++s_seq_sub_step;

  if (s_seq_sub_step >= 24) {
    s_seq_sub_step = 0;
    s_seq_step_clock = s_seq_step_clock_candidate;
  }

  if ((s_seq_sub_step % s_seq_step_clock) == 0) {
    bool update_scale = false;

    do {
      if (s_seq_mode == 0) {  // Forward
        ++s_seq_step;

        if (s_seq_step > s_seq_last_step) {
          s_seq_step = 0;
          update_scale = true;
        }

        s_seq_mode_dir = +1;
      } else if (s_seq_mode == 1) {  // Reverse
        --s_seq_step;

        if (s_seq_step < 0) {
          s_seq_step = s_seq_last_step;
          update_scale = true;
        }

        s_seq_mode_dir = -1;
      } else {  // Bounce
        if (s_seq_mode_dir > 0) {
          ++s_seq_step;

          if (s_seq_step > s_seq_last_step) {
            s_seq_step = s_seq_last_step;
            update_scale = true;
            s_seq_mode_dir = -1;
          } else {
            s_seq_mode_dir = +1;
          }
        } else {
          --s_seq_step;

          if (s_seq_step < 0) {
            s_seq_step = 0;
            update_scale = true;
            s_seq_mode_dir = +1;
          } else {
            s_seq_mode_dir = -1;
          }
        }
      }
    } while (((s_seq_step & 0x07) != 0) && (((1 << ((s_seq_step & 0x07) - 1)) & s_seq_act_steps) == 0));

    s_display_buffer[0][20] = '0' + (s_seq_step & 0x07);

    if (update_scale) {
      s_index_scale     = PRA32_U2_ControlPanel_get_index_scale();
      s_panel_pit_ofst  = g_synth.current_controller_value(PANEL_PIT_OFST ) - 64;
      s_panel_transpose = g_synth.current_controller_value(PANEL_TRANSPOSE) - 64;
      s_seq_pit_ofst    = g_synth.current_controller_value(SEQ_PIT_OFST   ) - 64;
    }

    PRA32_U2_ControlPanel_update_pitch(true);
  } else {
    uint32_t seq_gate_time_corrected = static_cast<uint32_t>(s_seq_gate_time) << 0;

    switch (s_seq_step_clock) {
    case 6:
      break;
    case 12:
      seq_gate_time_corrected = static_cast<uint32_t>(s_seq_gate_time) << 1;
      break;
    case 24:
      seq_gate_time_corrected = static_cast<uint32_t>(s_seq_gate_time) << 2;
      break;
    }

    if ((s_seq_sub_step % s_seq_step_clock) >= seq_gate_time_corrected) {
      s_panel_play_note_gate = false;
    }
  }
}

static INLINE void PRA32_U2_ControlPanel_seq_start() {
  if (g_synth.current_controller_value(SEQ_TRX_ST_SP  ) >= 64) {
#if defined(PRA32_U2_USE_USB_MIDI) && !defined(PRA32_U2_DISABLE_USB_MIDI_TRANSMITTION)
    USB_MIDI.sendRealTime(midi::Start);
#endif  // defined(PRA32_U2_USE_USB_MIDI) && !defined(PRA32_U2_DISABLE_USB_MIDI_TRANSMITTION)

#if defined(PRA32_U2_USE_UART_MIDI)
    UART_MIDI.sendRealTime(midi::Start);
#endif  // defined(PRA32_U2_USE_UART_MIDI)
  }

  s_playing_status = PlayingStatus_Seq;
  PRA32_U2_ControlPanel_request_st7789_redraw();

  if (s_seq_mode == 0) {  // Forward
    s_seq_step = 31;
    s_seq_mode_dir = +1;
  } else if (s_seq_mode == 1) {  // Reverse
    s_seq_step = 0;
    s_seq_mode_dir = -1;
  } else {  // Bounce
    s_seq_step = 0;
    s_seq_mode_dir = -1;
  }

  s_seq_sub_step = 23;
  s_panel_play_note_gate = false;
}

static INLINE void PRA32_U2_ControlPanel_seq_stop() {
  if (g_synth.current_controller_value(SEQ_TRX_ST_SP  ) >= 64) {
#if defined(PRA32_U2_USE_USB_MIDI) && !defined(PRA32_U2_DISABLE_USB_MIDI_TRANSMITTION)
    USB_MIDI.sendRealTime(midi::Stop);
#endif  // defined(PRA32_U2_USE_USB_MIDI) && !defined(PRA32_U2_DISABLE_USB_MIDI_TRANSMITTION)

#if defined(PRA32_U2_USE_UART_MIDI)
    UART_MIDI.sendRealTime(midi::Stop);
#endif  // defined(PRA32_U2_USE_UART_MIDI)
  }

  s_playing_status = PlayingStatus_Stop;
  PRA32_U2_ControlPanel_request_st7789_redraw();
  s_display_buffer[0][20] = ' ';
  s_panel_play_note_gate = false;
}

static INLINE void PRA32_U2_ControlPanel_update_control_seq() {
  if ((s_play_mode == 1) && (s_seq_clock_src_external == false)) {  // Seq Mode
    s_seq_count += s_seq_count_increment;

    if (s_seq_count >= 7680000 * 2) {
      s_seq_count -= 7680000 * 2;

      PRA32_U2_ControlPanel_seq_clock();
    }
  }
}

static INLINE boolean PRA32_U2_ControlPanel_update_control_adc(uint32_t adc_number) {
#if !defined(PRA32_U2_KEY_INPUT_PLAY_KEY_PIN)
  if (adc_number == 2) {
    return false;
  }
#endif  // defined(PRA32_U2_KEY_INPUT_PLAY_KEY_PIN)

  uint8_t adc_control_value_new = PRA32_U2_ControlPanel_adc_control_value_candidate(adc_number);

  if (s_adc_control_value[adc_number] != adc_control_value_new) {
    uint8_t adc_control_value_old = s_adc_control_value[adc_number];
    s_adc_control_value[adc_number] = adc_control_value_new;

#if defined(PRA32_U2_KEY_INPUT_SHIFT_KEY_PIN)
    uint32_t shift_key_pressed = digitalRead(PRA32_U2_KEY_INPUT_SHIFT_KEY_PIN) == PRA32_U2_KEY_INPUT_ACTIVE_LEVEL;
    if (shift_key_pressed) {
      if        ((adc_control_value_old <= 64) && (adc_control_value_new > 64)) {
        adc_control_value_new = 64;
      } else if ((adc_control_value_old >= 64) && (adc_control_value_new < 64)) {
        adc_control_value_new = 64;
      }
    }
#endif  // defined(PRA32_U2_KEY_INPUT_SHIFT_KEY_PIN)

    uint8_t current_controller_value = adc_control_value_new;
    if        (s_adc_control_target[adc_number] < 128) {
      current_controller_value = getCurrentControllerValue(((g_midi_ch + s_current_synth) & 0x0F) + 1, s_adc_control_target[adc_number]);
    } else if (s_adc_control_target[adc_number] < 128 + 64) {
      current_controller_value = g_synth.current_controller_value(s_adc_control_target[adc_number]);
    }

    if ((adc_control_value_old <= current_controller_value) &&
        (adc_control_value_new >= current_controller_value)) {
      s_adc_control_catched[adc_number] = true;
    }

    if ((adc_control_value_old >= current_controller_value) &&
        (adc_control_value_new <= current_controller_value)) {
      s_adc_control_catched[adc_number] = true;
    }


    if (s_adc_control_target[adc_number] < 128) {
      if (s_adc_control_catched[adc_number]) {
        handleControlChange(((g_midi_ch + s_current_synth) & 0x0F) + 1, s_adc_control_target[adc_number], adc_control_value_new);
      }
    } else if (s_adc_control_target[adc_number] < 128 + 64) {
      if (s_adc_control_catched[adc_number]) {
        g_synth.control_change(s_adc_control_target[adc_number], adc_control_value_new);
      }
    } else if ((s_adc_control_target[adc_number] >= RD_PROGRAM_0) && (s_adc_control_target[adc_number] <= RD_PROGRAM_15)) {
      static boolean s_ready_to_read[PROGRAM_NUMBER_MAX + 1] = {};
      uint8_t program_number_to_read = s_adc_control_target[adc_number] - RD_PROGRAM_0;
      if (s_adc_control_value[adc_number] <= 32) {
        s_ready_to_read[program_number_to_read] = true;
      } else if (s_ready_to_read[program_number_to_read] && (s_adc_control_value[adc_number] >= 96)) {
        PRA32_U2_ControlPanel_execute_action_target(s_adc_control_target[adc_number]);
        s_ready_to_read[program_number_to_read] = false;
      }
    } else if ((s_adc_control_target[adc_number] >= WR_PROGRAM_0) && (s_adc_control_target[adc_number] <= WR_PROGRAM_15)) {
      static boolean s_ready_to_write[PROGRAM_NUMBER_MAX + 1] = {};
      uint8_t program_number_to_write = s_adc_control_target[adc_number] - WR_PROGRAM_0;
      if (s_adc_control_value[adc_number] <= 32) {
        s_ready_to_write[program_number_to_write] = true;
      } else if (s_ready_to_write[program_number_to_write] && (s_adc_control_value[adc_number] >= 96)) {
        PRA32_U2_ControlPanel_execute_action_target(s_adc_control_target[adc_number]);
        s_ready_to_write[program_number_to_write] = false;
      }
    } else if (s_adc_control_target[adc_number] == RD_PANEL_PRMS) {
      static boolean s_ready_to_read_panel_prms;
      if (s_adc_control_value[adc_number] <= 32) {
        s_ready_to_read_panel_prms = true;
      } else if (s_ready_to_read_panel_prms && (s_adc_control_value[adc_number] >= 96)) {
        PRA32_U2_ControlPanel_execute_action_target(s_adc_control_target[adc_number]);
        s_ready_to_read_panel_prms = false;
      }
    } else if (s_adc_control_target[adc_number] == IN_PANEL_PRMS) {
      static boolean s_ready_to_init_panel_prms;
      if (s_adc_control_value[adc_number] <= 32) {
        s_ready_to_init_panel_prms = true;
      } else if (s_ready_to_init_panel_prms && (s_adc_control_value[adc_number] >= 96)) {
        PRA32_U2_ControlPanel_execute_action_target(s_adc_control_target[adc_number]);
        s_ready_to_init_panel_prms = false;
      }
    } else if (s_adc_control_target[adc_number] == WR_PANEL_PRMS) {
      static boolean s_ready_to_write_panel_prms;
      if (s_adc_control_value[adc_number] <= 32) {
        s_ready_to_write_panel_prms = true;
      } else if (s_ready_to_write_panel_prms && (s_adc_control_value[adc_number] >= 96)) {
        PRA32_U2_ControlPanel_execute_action_target(s_adc_control_target[adc_number]);
        s_ready_to_write_panel_prms = false;
      }
    } else if (s_adc_control_target[adc_number] == SEQ_RAND_PITCH) {
      static boolean s_ready_to_rand_pitch;
      if (s_adc_control_value[adc_number] <= 32) {
        s_ready_to_rand_pitch = true;
      } else if (s_ready_to_rand_pitch && (s_adc_control_value[adc_number] >= 96)) {
        PRA32_U2_ControlPanel_execute_action_target(s_adc_control_target[adc_number]);
        s_ready_to_rand_pitch = false;
      }
    } else if (s_adc_control_target[adc_number] == SEQ_RAND_VELO) {
      static boolean s_ready_to_rand_velo;
      if (s_adc_control_value[adc_number] <= 32) {
        s_ready_to_rand_velo = true;
      } else if (s_ready_to_rand_velo && (s_adc_control_value[adc_number] >= 96)) {
        PRA32_U2_ControlPanel_execute_action_target(s_adc_control_target[adc_number]);
        s_ready_to_rand_velo = false;
      }
    } else if (s_adc_control_target[adc_number] == PANIC_OP) {
      static boolean s_ready_to_panic;
      if (s_adc_control_value[adc_number] <= 32) {
        s_ready_to_panic = true;
      } else if (s_ready_to_panic && (s_adc_control_value[adc_number] >= 96)) {
        PRA32_U2_ControlPanel_execute_action_target(s_adc_control_target[adc_number]);
        s_ready_to_panic = false;
      }
    }

    return true;
  }

  return false;
}

static INLINE uint8_t PRA32_U2_ControlPanel_get_target_value(uint8_t target) {
  if (target < 128) {
    return getCurrentControllerValue(((g_midi_ch + s_current_synth) & 0x0F) + 1, target);
  }
  if (target < 128 + 64) {
    return g_synth.current_controller_value(target);
  }
  return 0;
}

static INLINE void PRA32_U2_ControlPanel_set_target_value(uint8_t target, uint8_t value) {
  if (target < 128) {
    handleControlChange(((g_midi_ch + s_current_synth) & 0x0F) + 1, target, value);
  } else if (target < 128 + 64) {
    g_synth.control_change(target, value);
  }
  PRA32_U2_ControlPanel_request_st7789_redraw();
}

static INLINE void PRA32_U2_ControlPanel_execute_action_target(uint8_t target) {
  if ((target >= RD_PROGRAM_0) && (target <= RD_PROGRAM_15)) {
    handleProgramChange(((g_midi_ch + s_current_synth) & 0x0F) + 1, target - RD_PROGRAM_0);
  } else if ((target >= WR_PROGRAM_0) && (target <= WR_PROGRAM_15)) {
    uint8_t program_number_to_write = target - WR_PROGRAM_0;
    writeParametersToProgram(((g_midi_ch + s_current_synth) & 0x0F) + 1, program_number_to_write);
  } else if (target == RD_PANEL_PRMS) {
    handleProgramChange(((g_midi_ch + s_current_synth) & 0x0F) + 1, 128);
  } else if (target == IN_PANEL_PRMS) {
    handleProgramChange(((g_midi_ch + s_current_synth) & 0x0F) + 1, 129);
  } else if (target == WR_PANEL_PRMS) {
    g_synth.write_parameters_to_program(128);
  } else if (target == SEQ_RAND_PITCH) {
    uint8_t array[8] = {};
    getRandUint8Rrray(((g_midi_ch + s_current_synth) & 0x0F) + 1, array);
    g_synth.control_change(SEQ_PITCH_0    , array[0] & 0x7Fu);
    g_synth.control_change(SEQ_PITCH_1    , array[1] & 0x7Fu);
    g_synth.control_change(SEQ_PITCH_2    , array[2] & 0x7Fu);
    g_synth.control_change(SEQ_PITCH_3    , array[3] & 0x7Fu);
    g_synth.control_change(SEQ_PITCH_4    , array[4] & 0x7Fu);
    g_synth.control_change(SEQ_PITCH_5    , array[5] & 0x7Fu);
    g_synth.control_change(SEQ_PITCH_6    , array[6] & 0x7Fu);
    g_synth.control_change(SEQ_PITCH_7    , array[7] & 0x7Fu);
  } else if (target == SEQ_RAND_VELO) {
    uint8_t array[8] = {};
    getRandUint8Rrray(((g_midi_ch + s_current_synth) & 0x0F) + 1, array);
    g_synth.control_change(SEQ_VELO_0     , array[0] & 0x7Fu);
    g_synth.control_change(SEQ_VELO_1     , array[1] & 0x7Fu);
    g_synth.control_change(SEQ_VELO_2     , array[2] & 0x7Fu);
    g_synth.control_change(SEQ_VELO_3     , array[3] & 0x7Fu);
    g_synth.control_change(SEQ_VELO_4     , array[4] & 0x7Fu);
    g_synth.control_change(SEQ_VELO_5     , array[5] & 0x7Fu);
    g_synth.control_change(SEQ_VELO_6     , array[6] & 0x7Fu);
    g_synth.control_change(SEQ_VELO_7     , array[7] & 0x7Fu);
  } else if (target == PANIC_OP) {
    handleControlChange(((g_midi_ch + s_current_synth) & 0x0F) + 1, ALL_SOUND_OFF  , 0);
    handleControlChange(((g_midi_ch + s_current_synth) & 0x0F) + 1, RESET_ALL_CTRLS, 0);
  }
  PRA32_U2_ControlPanel_request_st7789_redraw();
}

static INLINE void PRA32_U2_ControlPanel_build_short_label(const char line0[11], const char line1[11], char out[11]) {
  const char* source = line1;
  bool line1_has_text = false;
  for (uint8_t i = 0; i < 10; ++i) {
    if ((line1[i] != ' ') && (line1[i] != '\0')) {
      line1_has_text = true;
      break;
    }
  }
  if (!line1_has_text) {
    source = line0;
  }

  uint8_t end = 10;
  while ((end > 0) && ((source[end - 1] == ' ') || (source[end - 1] == '\0'))) {
    --end;
  }

  std::memset(out, 0, 11);
  for (uint8_t i = 0; i < end; ++i) {
    out[i] = source[i];
  }
}

static volatile bool s_st7789_redraw_requested = true;

static INLINE void PRA32_U2_ControlPanel_request_st7789_redraw() {
  s_st7789_redraw_requested = true;
}

static INLINE bool PRA32_U2_ControlPanel_st7789_target_is_visible(uint8_t target) {
  PRA32_U2_ControlPanelPage current_page = g_control_panel_page_table[s_current_page_group][s_current_page_index[s_current_page_group]];
  uint8_t target_c = current_page.control_target_c;
  if (s_play_mode == 1) {
    target_c = SEQ_PIT_OFST;
  }

  return (current_page.control_target_a == target) ||
         (current_page.control_target_b == target) ||
         (target_c == target);
}

static INLINE void PRA32_U2_ControlPanel_build_st7789_frame(PRA32_U2_UI_RenderFrame& frame) {
  std::memset(&frame, 0, sizeof(frame));

  PRA32_U2_ControlPanelPage current_page = g_control_panel_page_table[s_current_page_group][s_current_page_index[s_current_page_group]];
  if (s_play_mode == 1) {
    std::memcpy(current_page.control_target_c_name_line_0, "Seq       ", 10);
    std::memcpy(current_page.control_target_c_name_line_1, "Pitch Ofst", 10);
    current_page.control_target_c = SEQ_PIT_OFST;
  }

  frame.page_group = static_cast<uint8_t>(s_current_page_group);
  frame.page_index = static_cast<uint8_t>(s_current_page_index[s_current_page_group]);
  frame.page_count = static_cast<uint8_t>(g_number_of_pages[s_current_page_group]);

  std::snprintf(frame.page_name, sizeof(frame.page_name), "%-10.10s %-10.10s",
                current_page.page_name_line_0, current_page.page_name_line_1);
  std::snprintf(frame.mode_text, sizeof(frame.mode_text), "%s", s_play_mode == 1 ? "Seq" : "Nrm");

  if (s_playing_status == PlayingStatus_Seq) {
    std::snprintf(frame.status_text, sizeof(frame.status_text), "RUN");
  } else if (s_playing_status == PlayingStatus_Playing) {
    std::snprintf(frame.status_text, sizeof(frame.status_text), "PLY");
  } else {
    std::snprintf(frame.status_text, sizeof(frame.status_text), "STP");
  }

  PRA32_U2_UI_StateSnapshot snapshot = PRA32_U2_UI_StateMachine_snapshot();
  PRA32_U2_UI_FocusItem focused_item = PRA32_U2_UI_StateMachine_focused_item();
  frame.state = snapshot.state;
  frame.confirm_selected = snapshot.confirm_selected;

  const uint8_t targets[3] = {
    current_page.control_target_a,
    current_page.control_target_b,
    current_page.control_target_c
  };
  const char* line0[3] = {
    current_page.control_target_a_name_line_0,
    current_page.control_target_b_name_line_0,
    current_page.control_target_c_name_line_0
  };
  const char* line1[3] = {
    current_page.control_target_a_name_line_1,
    current_page.control_target_b_name_line_1,
    current_page.control_target_c_name_line_1
  };

  for (uint8_t i = 0; i < 3; ++i) {
    PRA32_U2_UI_RenderItem& item = frame.items[i];
    item.visible = (targets[i] != 0xFF);
    item.source_index = i;
    item.target = targets[i];
    item.focused = (snapshot.focused_item_count > 0) &&
                   (snapshot.focused_item_index < snapshot.focused_item_count) &&
                   (focused_item.source_index == i);

    if (!item.visible) {
      continue;
    }

    item.type = focused_item.type;
    if (!item.focused) {
      if (PRA32_U2_UI_StateMachine_is_dangerous_action_target(targets[i])) {
        item.type = PRA32_U2_UI_FocusItemType_Action;
      } else {
        item.type = PRA32_U2_UI_FocusItemType_Parameter;
      }
    }

    item.value = PRA32_U2_ControlPanel_get_target_value(targets[i]);
    char value_display_text[5] = {};
    item.has_value_text = PRA32_U2_ControlPanel_calc_value_display(targets[i], item.value, value_display_text);
    if (item.has_value_text) {
      std::memcpy(item.value_text, value_display_text, sizeof(item.value_text));
    }

    PRA32_U2_ControlPanel_build_short_label(line0[i], line1[i], item.short_label);
  }
}

INLINE void PRA32_U2_ControlPanel_setup() {
  s_current_program[0] = (PROGRAM_NUMBER_DEFAULT + ((PRA32_U2_NUMBER_OF_SYNTHS > 1) * 4) + 0) & USER_PROGRAM_NUMBER_MAX;
  for (uint32_t i = 1; i < PRA32_U2_NUMBER_OF_SYNTHS; ++i) {
    s_current_program[i] = (s_current_program[i - 1] + 1) & USER_PROGRAM_NUMBER_MAX;
  }

#if defined(PRA32_U2_KEY_INPUT_PREV_KEY_PIN)
  pinMode(PRA32_U2_KEY_INPUT_PREV_KEY_PIN, PRA32_U2_KEY_INPUT_PIN_MODE);
#endif  // defined(PRA32_U2_KEY_INPUT_PREV_KEY_PIN)

#if defined(PRA32_U2_KEY_INPUT_NEXT_KEY_PIN)
  pinMode(PRA32_U2_KEY_INPUT_NEXT_KEY_PIN, PRA32_U2_KEY_INPUT_PIN_MODE);
#endif  // defined(PRA32_U2_KEY_INPUT_NEXT_KEY_PIN)

#if defined(PRA32_U2_KEY_INPUT_PLAY_KEY_PIN)
  pinMode(PRA32_U2_KEY_INPUT_PLAY_KEY_PIN, PRA32_U2_KEY_INPUT_PIN_MODE);
#endif  // defined(PRA32_U2_KEY_INPUT_PLAY_KEY_PIN)

#if defined(PRA32_U2_KEY_INPUT_PROG_MINUS_KEY_PIN)
  pinMode(PRA32_U2_KEY_INPUT_PROG_MINUS_KEY_PIN, PRA32_U2_KEY_INPUT_PIN_MODE);
#endif  // defined(PRA32_U2_KEY_INPUT_PROG_MINUS_KEY_PIN)

#if defined(PRA32_U2_KEY_INPUT_PROG_PLUS_KEY_PIN)
  pinMode(PRA32_U2_KEY_INPUT_PROG_PLUS_KEY_PIN, PRA32_U2_KEY_INPUT_PIN_MODE);
#endif  // defined(PRA32_U2_KEY_INPUT_PROG_PLUS_KEY_PIN)

#if defined(PRA32_U2_KEY_INPUT_SHIFT_KEY_PIN)
  pinMode(PRA32_U2_KEY_INPUT_SHIFT_KEY_PIN, PRA32_U2_KEY_INPUT_PIN_MODE);
#endif  // defined(PRA32_U2_KEY_INPUT_SHIFT_KEY_PIN)

#if defined(PRA32_U2_USE_CONTROL_PANEL)
  PRA32_U2_ControlPanel_update_page();

#if defined(PRA32_U2_USE_CONTROL_PANEL_ENCODER_INPUT)
  PRA32_U2_UI_EncoderInput_setup();
  PRA32_U2_UI_StateMachine_initialize(PRA32_U2_ControlPanel_update_page);
#endif

#if defined(PRA32_U2_USE_CONTROL_PANEL_ANALOG_INPUT)
  adc_init();
  adc_gpio_init(26);
  adc_gpio_init(27);

#if defined(PRA32_U2_KEY_INPUT_PLAY_KEY_PIN)
  adc_gpio_init(28);
#endif  // defined(PRA32_U2_KEY_INPUT_PLAY_KEY_PIN)

#endif  // defined(PRA32_U2_USE_CONTROL_PANEL_ANALOG_INPUT)

#if defined(PRA32_U2_USE_CONTROL_PANEL_OLED_DISPLAY)
  i2c_init(PRA32_U2_OLED_DISPLAY_I2C, 400 * 1000);
  i2c_set_slave_mode(PRA32_U2_OLED_DISPLAY_I2C, false, 0);
  gpio_set_function(PRA32_U2_OLED_DISPLAY_I2C_SDA_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(PRA32_U2_OLED_DISPLAY_I2C_SDA_PIN);
  gpio_set_function(PRA32_U2_OLED_DISPLAY_I2C_SCL_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(PRA32_U2_OLED_DISPLAY_I2C_SCL_PIN);

  uint8_t commands_init_0[] = {0x00,  0x81, PRA32_U2_OLED_DISPLAY_CONTRAST,  0xA0, 0xC0,  0x8D, 0x14};
  if (PRA32_U2_OLED_DISPLAY_ROTATION) {
    commands_init_0[3] = 0xA1;
    commands_init_0[4] = 0xC8;
  }

  i2c_write_blocking(PRA32_U2_OLED_DISPLAY_I2C, PRA32_U2_OLED_DISPLAY_I2C_ADDRESS, commands_init_0, sizeof(commands_init_0), false);

  for (uint8_t y = 0; y <= 7; ++y) {
    for (uint8_t x = 0; x <= 20; ++x) {
      PRA32_U2_ControlPanel_set_draw_position(x, y);
      PRA32_U2_ControlPanel_draw_character(s_display_buffer[y][x]);
    }

    uint8_t commands[] = {0x00,  static_cast<uint8_t>(0xB0 + y), 0x17, 0x0E};
    i2c_write_blocking(PRA32_U2_OLED_DISPLAY_I2C, PRA32_U2_OLED_DISPLAY_I2C_ADDRESS, commands, sizeof(commands), false);

    uint8_t data[] = {0x40,  0x00, 0x00};
    i2c_write_blocking(PRA32_U2_OLED_DISPLAY_I2C, PRA32_U2_OLED_DISPLAY_I2C_ADDRESS, data, sizeof(data), false);
  }

  uint8_t commands_init_1[] = {0x00,  0xAF};
  i2c_write_blocking(PRA32_U2_OLED_DISPLAY_I2C, PRA32_U2_OLED_DISPLAY_I2C_ADDRESS, commands_init_1, sizeof(commands_init_1), false);
#endif  // defined(PRA32_U2_USE_CONTROL_PANEL_OLED_DISPLAY)

#if defined(PRA32_U2_USE_CONTROL_PANEL_ST7789_DISPLAY)
  PRA32_U2_UI_RenderST7789_setup();
#endif  // defined(PRA32_U2_USE_CONTROL_PANEL_ST7789_DISPLAY)

#endif  // defined(PRA32_U2_USE_CONTROL_PANEL)
}

INLINE void PRA32_U2_ControlPanel_initialize_parameters() {
}

INLINE void PRA32_U2_ControlPanel_update_analog_inputs(uint32_t loop_counter) {
#if defined(PRA32_U2_USE_CONTROL_PANEL_ENCODER_INPUT)
  static_cast<void>(loop_counter);
#else
  PRA32_U2_UI_InputLegacy_update_analog_inputs(loop_counter, s_adc_current_value);
#endif
}

INLINE void PRA32_U2_ControlPanel_update_control() {
#if defined(PRA32_U2_USE_CONTROL_PANEL)
  static uint32_t s_initialize_counter = 0;
  if (s_initialize_counter < 75) {
    ++s_initialize_counter;
#if defined(PRA32_U2_USE_CONTROL_PANEL_ANALOG_INPUT)
    s_adc_control_value[0] = PRA32_U2_ControlPanel_adc_control_value_candidate(0);
    s_adc_control_value[1] = PRA32_U2_ControlPanel_adc_control_value_candidate(1);
    s_adc_control_value[2] = PRA32_U2_ControlPanel_adc_control_value_candidate(2);
#endif  // defined(PRA32_U2_USE_CONTROL_PANEL_ANALOG_INPUT)
    return;
  }

  PRA32_U2_ControlPanel_update_control_seq();

  boolean processed = PRA32_U2_ControlPanel_process_note_off_on();
  if (processed) {
    return;
  }

#if defined(PRA32_U2_USE_CONTROL_PANEL_ENCODER_INPUT)
  PRA32_U2_UI_EncoderInputEvent event = PRA32_U2_UI_EncoderInput_poll();
  if ((event.rotation_delta != 0) || event.short_click || event.long_click) {
    PRA32_U2_ControlPanel_request_st7789_redraw();
  }
  PRA32_U2_UI_StateMachine_process_event(event,
                                         PRA32_U2_ControlPanel_get_target_value,
                                         PRA32_U2_ControlPanel_set_target_value,
                                         PRA32_U2_ControlPanel_execute_action_target,
                                         PRA32_U2_ControlPanel_update_page);
#endif
#if defined(PRA32_U2_USE_CONTROL_PANEL_KEY_INPUT)
  static uint32_t s_prev_key_value_changed_time = 0;
  static uint32_t s_next_key_value_changed_time = 0;
  static uint32_t s_play_key_value_changed_time = 0;

  static uint32_t s_prog_minus_key_value_changed_time = 0;
  static uint32_t s_prog_plus_key_value_changed_time  = 0;

  static uint32_t s_prev_key_long_pressed = false;
  static uint32_t s_next_key_long_pressed = false;

  static uint32_t s_prog_minus_key_long_pressed = false;
  static uint32_t s_prog_plus_key_long_pressed  = false;

  static uint32_t s_key_inpuy_counter = 0;
  ++s_key_inpuy_counter;

#if defined(PRA32_U2_KEY_INPUT_PREV_KEY_PIN)
  if (s_key_inpuy_counter - s_prev_key_value_changed_time >= PRA32_U2_KEY_ANTI_CHATTERING_WAIT) {
    uint32_t value = digitalRead(PRA32_U2_KEY_INPUT_PREV_KEY_PIN) == PRA32_U2_KEY_INPUT_ACTIVE_LEVEL;

    if (s_prev_key_current_value != value) {
      s_prev_key_current_value = value;
      s_prev_key_value_changed_time = s_key_inpuy_counter;

      if (s_prev_key_current_value == 0) {
        // Prev key released
        if (s_prev_key_long_pressed == false) {
#if defined(PRA32_U2_KEY_INPUT_SHIFT_KEY_PIN)
          uint32_t shift_key_pressed = digitalRead(PRA32_U2_KEY_INPUT_SHIFT_KEY_PIN) == PRA32_U2_KEY_INPUT_ACTIVE_LEVEL;
          if (shift_key_pressed) {
            if (s_current_page_group == 0) {
              s_current_page_group = NUMBER_OF_PAGE_GROUPS - 1;
            } else {
              --s_current_page_group;
            }

            PRA32_U2_ControlPanel_update_page();
            return;
          }

#endif  // defined(PRA32_U2_KEY_INPUT_SHIFT_KEY_PIN)
          if (s_current_page_index[s_current_page_group] == 0) {
            s_current_page_index[s_current_page_group] = g_number_of_pages[s_current_page_group] - 1;
          } else {
            --s_current_page_index[s_current_page_group];
          }

          PRA32_U2_ControlPanel_update_page();
          return;
        }

        s_prev_key_long_pressed = false;
        return;
      }

#if (PRA32_U2_NUMBER_OF_SYNTHS > 1)
      if (s_prev_key_current_value == 1) {
        // Prev key pressed
        if (s_prev_key_long_pressed == false) {
#if defined(PRA32_U2_KEY_INPUT_NEXT_KEY_PIN)
          uint32_t next_key_pressed = digitalRead(PRA32_U2_KEY_INPUT_NEXT_KEY_PIN) == PRA32_U2_KEY_INPUT_ACTIVE_LEVEL;
          if (next_key_pressed) {
            if (s_panel_playing_note_pitch <= 127) {
              handleNoteOff(((g_midi_ch + s_current_synth) & 0x0F) + 1, s_panel_playing_note_pitch, 64);
            }

            s_current_synth = (s_current_synth + 1) >= PRA32_U2_NUMBER_OF_SYNTHS ? 0 : (s_current_synth + 1);
            s_display_buffer[0][14] = '0' + s_current_synth;

            s_prev_key_long_pressed = true;
            s_next_key_long_pressed = true;

            PRA32_U2_ControlPanel_update_page();
            return;
          }

#endif  // defined(PRA32_U2_KEY_INPUT_PREV_KEY_PIN)
        }
      }
#endif  // (PRA32_U2_NUMBER_OF_SYNTHS > 1)
    }

    if (s_prev_key_current_value == 1) {
      if (s_prev_key_long_pressed == false) {
        if (s_key_inpuy_counter - s_prev_key_value_changed_time >= PRA32_U2_KEY_LONG_PRESS_WAIT) {
          s_prev_key_long_pressed = true;

          if (s_current_page_group == 0) {
            s_current_page_group = NUMBER_OF_PAGE_GROUPS - 1;
          } else {
            --s_current_page_group;
          }

          PRA32_U2_ControlPanel_update_page();
          return;
        }
      }
    }
  }
#endif  // defined(PRA32_U2_KEY_INPUT_PREV_KEY_PIN)

#if defined(PRA32_U2_KEY_INPUT_NEXT_KEY_PIN)
  if (s_key_inpuy_counter - s_next_key_value_changed_time >= PRA32_U2_KEY_ANTI_CHATTERING_WAIT) {
    uint32_t value = digitalRead(PRA32_U2_KEY_INPUT_NEXT_KEY_PIN) == PRA32_U2_KEY_INPUT_ACTIVE_LEVEL;

    if (s_next_key_current_value != value) {
      s_next_key_current_value = value;
      s_next_key_value_changed_time = s_key_inpuy_counter;

      if (s_next_key_current_value == 0) {
        // Next key released
        if (s_next_key_long_pressed == false) {
#if defined(PRA32_U2_KEY_INPUT_SHIFT_KEY_PIN)
          uint32_t shift_key_pressed = digitalRead(PRA32_U2_KEY_INPUT_SHIFT_KEY_PIN) == PRA32_U2_KEY_INPUT_ACTIVE_LEVEL;
          if (shift_key_pressed) {
            if (s_current_page_group == NUMBER_OF_PAGE_GROUPS - 1) {
              s_current_page_group = 0;
            } else {
              ++s_current_page_group;
            }

            PRA32_U2_ControlPanel_update_page();
            return;
          }

#endif  // defined(PRA32_U2_KEY_INPUT_SHIFT_KEY_PIN)
          if (s_current_page_index[s_current_page_group] == g_number_of_pages[s_current_page_group] - 1) {
            s_current_page_index[s_current_page_group] = 0;
          } else {
            ++s_current_page_index[s_current_page_group];
          }

          PRA32_U2_ControlPanel_update_page();
          return;
        }

        s_next_key_long_pressed = false;
        return;
      }

#if (PRA32_U2_NUMBER_OF_SYNTHS > 1)
      if (s_next_key_current_value == 1) {
        // Next key pressed
        if (s_next_key_long_pressed == false) {
#if defined(PRA32_U2_KEY_INPUT_PREV_KEY_PIN)
          uint32_t prev_key_pressed = digitalRead(PRA32_U2_KEY_INPUT_PREV_KEY_PIN) == PRA32_U2_KEY_INPUT_ACTIVE_LEVEL;
          if (prev_key_pressed) {
            if (s_panel_playing_note_pitch <= 127) {
              handleNoteOff(((g_midi_ch + s_current_synth) & 0x0F) + 1, s_panel_playing_note_pitch, 64);
            }

            s_current_synth = (s_current_synth + 1) >= PRA32_U2_NUMBER_OF_SYNTHS ? 0 : (s_current_synth + 1);
            s_display_buffer[0][14] = '0' + s_current_synth;

            s_prev_key_long_pressed = true;
            s_next_key_long_pressed = true;

            PRA32_U2_ControlPanel_update_page();
            return;
          }

#endif  // defined(PRA32_U2_KEY_INPUT_PREV_KEY_PIN)
        }
      }
#endif  // (PRA32_U2_NUMBER_OF_SYNTHS > 1)
    }

    if (s_next_key_current_value == 1) {
      if (s_next_key_long_pressed == false) {
        if (s_key_inpuy_counter - s_next_key_value_changed_time >= PRA32_U2_KEY_LONG_PRESS_WAIT) {
          s_next_key_long_pressed = true;

          if (s_current_page_group == NUMBER_OF_PAGE_GROUPS - 1) {
            s_current_page_group = 0;
          } else {
            ++s_current_page_group;
          }

          PRA32_U2_ControlPanel_update_page();
          return;
        }
      }
    }
  }
#endif  // defined(PRA32_U2_KEY_INPUT_NEXT_KEY_PIN)

#if defined(PRA32_U2_KEY_INPUT_PLAY_KEY_PIN)
  if (s_key_inpuy_counter - s_play_key_value_changed_time >= PRA32_U2_KEY_ANTI_CHATTERING_WAIT) {
    uint32_t value = digitalRead(PRA32_U2_KEY_INPUT_PLAY_KEY_PIN) == PRA32_U2_KEY_INPUT_ACTIVE_LEVEL;

    if (s_play_key_current_value != value) {
      s_play_key_current_value = value;
      s_play_key_value_changed_time = s_key_inpuy_counter;

      if (s_play_key_current_value == 1) {
        // Play key pressed
        if (s_play_mode == 0) {  // Normal Mode
          s_playing_status = PlayingStatus_Playing;
          s_display_buffer[0][20] = '*';
          s_panel_play_note_gate    = true;
          s_panel_play_note_trigger = true;
          PRA32_U2_ControlPanel_request_st7789_redraw();
        }
      } else {
        // Play key released
        if (s_play_mode == 0) {  // Normal Mode
          s_playing_status = PlayingStatus_Stop;
          s_display_buffer[0][20] = ' ';
          s_panel_play_note_gate = false;
          PRA32_U2_ControlPanel_request_st7789_redraw();
        } else {  // Seq Mode
          if (s_playing_status == PlayingStatus_Stop) {
            PRA32_U2_ControlPanel_seq_start();
          } else {
            PRA32_U2_ControlPanel_seq_stop();
          }
        }
      }

      return;
    }
  }
#endif  // defined(PRA32_U2_KEY_INPUT_PLAY_KEY_PIN)

#if defined(PRA32_U2_KEY_INPUT_PROG_MINUS_KEY_PIN)
  if (s_key_inpuy_counter - s_prog_minus_key_value_changed_time >= PRA32_U2_KEY_ANTI_CHATTERING_WAIT) {
    uint32_t value = digitalRead(PRA32_U2_KEY_INPUT_PROG_MINUS_KEY_PIN) == PRA32_U2_KEY_INPUT_ACTIVE_LEVEL;
    if (s_prog_minus_key_current_value != value) {
      s_prog_minus_key_current_value = value;
      s_prog_minus_key_value_changed_time = s_key_inpuy_counter;
      if (s_prog_minus_key_current_value == 0) {
        // Prog - key released
        if (s_prog_minus_key_long_pressed == false) {
#if defined(PRA32_U2_KEY_INPUT_SHIFT_KEY_PIN)
          uint32_t shift_key_pressed = digitalRead(PRA32_U2_KEY_INPUT_SHIFT_KEY_PIN) == PRA32_U2_KEY_INPUT_ACTIVE_LEVEL;
          if (shift_key_pressed) {
            if (s_current_program[s_current_synth] == 0) {
              s_current_program[s_current_synth] = USER_PROGRAM_NUMBER_MAX;
            } else {
              --s_current_program[s_current_synth];
            }
            handleProgramChange(((g_midi_ch + s_current_synth) & 0x0F) + 1, s_current_program[s_current_synth]);
            s_display_buffer[0][17] = '0' + s_current_program[s_current_synth];
          }
#endif  // defined(PRA32_U2_KEY_INPUT_SHIFT_KEY_PIN)
        }
        s_prog_minus_key_long_pressed = false;
        return;
      }
    }
    if (s_prog_minus_key_current_value == 1) {
      if (s_prog_minus_key_long_pressed == false) {
        if (s_key_inpuy_counter - s_prog_minus_key_value_changed_time >= PRA32_U2_KEY_LONG_PRESS_WAIT) {
          s_prog_minus_key_long_pressed = true;
          if (s_current_program[s_current_synth] == 0) {
            s_current_program[s_current_synth] = USER_PROGRAM_NUMBER_MAX;
          } else {
            --s_current_program[s_current_synth];
          }
          handleProgramChange(((g_midi_ch + s_current_synth) & 0x0F) + 1, s_current_program[s_current_synth]);
          s_display_buffer[0][17] = '0' + s_current_program[s_current_synth];
          return;
        }
      }
    }
  }
#endif  // defined(PRA32_U2_KEY_INPUT_PROG_MINUS_KEY_PIN)

#if defined(PRA32_U2_KEY_INPUT_PROG_PLUS_KEY_PIN)
  if (s_key_inpuy_counter - s_prog_plus_key_value_changed_time >= PRA32_U2_KEY_ANTI_CHATTERING_WAIT) {
    uint32_t value = digitalRead(PRA32_U2_KEY_INPUT_PROG_PLUS_KEY_PIN) == PRA32_U2_KEY_INPUT_ACTIVE_LEVEL;
    if (s_prog_plus_key_current_value != value) {
      s_prog_plus_key_current_value = value;
      s_prog_plus_key_value_changed_time = s_key_inpuy_counter;
      if (s_prog_plus_key_current_value == 0) {
        // Prog + key released
        if (s_prog_plus_key_long_pressed == false) {
#if defined(PRA32_U2_KEY_INPUT_SHIFT_KEY_PIN)
          uint32_t shift_key_pressed = digitalRead(PRA32_U2_KEY_INPUT_SHIFT_KEY_PIN) == PRA32_U2_KEY_INPUT_ACTIVE_LEVEL;
          if (shift_key_pressed) {
            if (s_current_program[s_current_synth] == USER_PROGRAM_NUMBER_MAX) {
              s_current_program[s_current_synth] = 0;
            } else {
              ++s_current_program[s_current_synth];
            }
            handleProgramChange(((g_midi_ch + s_current_synth) & 0x0F) + 1, s_current_program[s_current_synth]);
            s_display_buffer[0][17] = '0' + s_current_program[s_current_synth];
          }
#endif  // defined(PRA32_U2_KEY_INPUT_SHIFT_KEY_PIN)
        }
        s_prog_plus_key_long_pressed = false;
        return;
      }
    }
    if (s_prog_plus_key_current_value == 1) {
      if (s_prog_plus_key_long_pressed == false) {
        if (s_key_inpuy_counter - s_prog_plus_key_value_changed_time >= PRA32_U2_KEY_LONG_PRESS_WAIT) {
          s_prog_plus_key_long_pressed = true;
          if (s_current_program[s_current_synth] == USER_PROGRAM_NUMBER_MAX) {
            s_current_program[s_current_synth] = 0;
          } else {
            ++s_current_program[s_current_synth];
          }
          handleProgramChange(((g_midi_ch + s_current_synth) & 0x0F) + 1, s_current_program[s_current_synth]);
          s_display_buffer[0][17] = '0' + s_current_program[s_current_synth];
          return;
        }
      }
    }
  }
#endif  // defined(PRA32_U2_KEY_INPUT_PROG_PLUS_KEY_PIN)

#endif  // defined(PRA32_U2_USE_CONTROL_PANEL_KEY_INPUT)

#if defined(PRA32_U2_USE_CONTROL_PANEL_ANALOG_INPUT)
  static uint32_t s_adc_number_to_check = 0;

  boolean updated = PRA32_U2_ControlPanel_update_control_adc(s_adc_number_to_check);
  s_adc_number_to_check = (s_adc_number_to_check + 1) % 3;

  if (updated == false) {
    updated = PRA32_U2_ControlPanel_update_control_adc(s_adc_number_to_check);
    s_adc_number_to_check = (s_adc_number_to_check + 1) % 3;

    if (updated == false) {
      updated = PRA32_U2_ControlPanel_update_control_adc(s_adc_number_to_check);
      s_adc_number_to_check = (s_adc_number_to_check + 1) % 3;
    }
  }
#endif  // defined(PRA32_U2_USE_CONTROL_PANEL_ANALOG_INPUT)

#endif  // defined(PRA32_U2_USE_CONTROL_PANEL)
}


INLINE void PRA32_U2_ControlPanel_on_clock()
{
  if (s_seq_clock_src_external) {
    if (s_play_mode == 1) {  // Seq Mode
      PRA32_U2_ControlPanel_seq_clock();
    }
  }
}

INLINE void PRA32_U2_ControlPanel_on_start()
{
  if (s_seq_clock_src_external) {
    if (s_play_mode == 1) {  // Seq Mode
      if (s_playing_status == PlayingStatus_Stop) {
        PRA32_U2_ControlPanel_seq_start();
      }
    }
  }
}

INLINE void PRA32_U2_ControlPanel_on_stop()
{
  if (s_seq_clock_src_external) {
    if (s_play_mode == 1) {  // Seq Mode
      if (s_playing_status != PlayingStatus_Stop) {
        PRA32_U2_ControlPanel_seq_stop();
      }
    }
  }
}

INLINE void PRA32_U2_ControlPanel_update_display_buffer(uint32_t loop_counter) {
  static_cast<void>(loop_counter);

#if defined(PRA32_U2_USE_CONTROL_PANEL)
  if ((loop_counter & 0x7F) == 0x00) {
    char buff[6];

#if defined(PRA32_U2_USE_CONTROL_PANEL_ENCODER_INPUT)
    PRA32_U2_UI_StateSnapshot snapshot = PRA32_U2_UI_StateMachine_snapshot();
    PRA32_U2_UI_FocusItem focus_item = PRA32_U2_UI_StateMachine_focused_item();
    const char* state_text = "GROUP";
    switch (snapshot.state) {
    case PRA32_U2_UI_State_GroupNavigation: state_text = "GROUP"; break;
    case PRA32_U2_UI_State_PageNavigation:  state_text = "PAGE "; break;
    case PRA32_U2_UI_State_ItemNavigation:  state_text = "ITEM "; break;
    case PRA32_U2_UI_State_ItemEdit:        state_text = "EDIT "; break;
    case PRA32_U2_UI_State_ActionConfirm:   state_text = "CONF "; break;
    }
    std::memcpy(&s_display_buffer[0][0], "UI:", 3);
    std::memcpy(&s_display_buffer[0][3], state_text, 5);
    s_display_buffer[0][8] = snapshot.confirm_selected ? 'Y' : 'N';
    s_display_buffer[0][9] = ' ';
    s_display_buffer[5][10] = ' ';
    s_display_buffer[5][20] = ' ';
    s_display_buffer[1][20] = ' ';
    if (focus_item.source_index == 0) {
      s_display_buffer[5][10] = '>';
    } else if (focus_item.source_index == 1) {
      s_display_buffer[5][20] = '>';
    } else if (focus_item.source_index == 2) {
      s_display_buffer[1][20] = '>';
    }
#endif

    uint8_t adc_control_target_0 = s_adc_control_target[0];
    if (adc_control_target_0 < 0xFF) {
      uint8_t adc_control_value        = s_adc_control_value[0];
      uint8_t current_controller_value = adc_control_value;
      if        (adc_control_target_0 < 128) {
        current_controller_value = getCurrentControllerValue(((g_midi_ch + s_current_synth) & 0x0F) + 1, adc_control_target_0);
      } else if (adc_control_target_0 < 128 + 64) {
        current_controller_value = g_synth.current_controller_value(adc_control_target_0);
      }

      s_display_buffer[7][ 0] = 'A';
      if        (adc_control_value < current_controller_value) {
        s_display_buffer[7][ 1] = '<';
      } else if (adc_control_value > current_controller_value) {
        s_display_buffer[7][ 1] = '>';
      } else {
        s_display_buffer[7][ 1] = '=';
      }

      std::snprintf(buff, sizeof(buff), "%3u", current_controller_value);
      s_display_buffer[7][ 2] = buff[0];
      s_display_buffer[7][ 3] = buff[1];
      s_display_buffer[7][ 4] = buff[2];

      char value_display_text[5] = {};
      boolean exists = PRA32_U2_ControlPanel_calc_value_display(adc_control_target_0, current_controller_value, value_display_text);
      if (exists) {
        s_display_buffer[7][ 5] = '[';
        s_display_buffer[7][ 6] = value_display_text[0];
        s_display_buffer[7][ 7] = value_display_text[1];
        s_display_buffer[7][ 8] = value_display_text[2];
        s_display_buffer[7][ 9] = ']';
      } else {
        std::memset(&s_display_buffer[7][ 5], ' ', 5);
      }
    } else {
      std::memset(&s_display_buffer[7][ 0], ' ', 10);
    }

    uint8_t adc_control_target_1 = s_adc_control_target[1];
    if (adc_control_target_1 < 0xFF) {
      uint8_t adc_control_value        = s_adc_control_value[1];
      uint8_t current_controller_value = adc_control_value;
      if        (adc_control_target_1 < 128) {
        current_controller_value = getCurrentControllerValue(((g_midi_ch + s_current_synth) & 0x0F) + 1, adc_control_target_1);
      } else if (adc_control_target_1 < 128 + 64) {
        current_controller_value = g_synth.current_controller_value(adc_control_target_1);
      }

      s_display_buffer[7][11] = 'B';
      if        (adc_control_value < current_controller_value) {
        s_display_buffer[7][12] = '<';
      } else if (adc_control_value > current_controller_value) {
        s_display_buffer[7][12] = '>';
      } else {
        s_display_buffer[7][12] = '=';
      }

      std::snprintf(buff, sizeof(buff), "%3u", current_controller_value);
      s_display_buffer[7][13] = buff[0];
      s_display_buffer[7][14] = buff[1];
      s_display_buffer[7][15] = buff[2];

      char value_display_text[5] = {};
      boolean exists = PRA32_U2_ControlPanel_calc_value_display(adc_control_target_1, current_controller_value, value_display_text);
      if (exists) {
        s_display_buffer[7][16] = '[';
        s_display_buffer[7][17] = value_display_text[0];
        s_display_buffer[7][18] = value_display_text[1];
        s_display_buffer[7][19] = value_display_text[2];
        s_display_buffer[7][20] = ']';
      } else {
        std::memset(&s_display_buffer[7][16], ' ', 5);
      }
    } else {
      std::memset(&s_display_buffer[7][11], ' ', 10);
    }

    uint8_t adc_control_target_2 = s_adc_control_target[2];
    if (adc_control_target_2 < 0xFF) {
      uint8_t adc_control_value        = s_adc_control_value[2];
      uint8_t current_controller_value = adc_control_value;
      if        (adc_control_target_2 < 128) {
        current_controller_value = getCurrentControllerValue(((g_midi_ch + s_current_synth) & 0x0F) + 1, adc_control_target_2);
      } else if (adc_control_target_2 < 128 + 64) {
        current_controller_value = g_synth.current_controller_value(adc_control_target_2);
      }

      s_display_buffer[3][11] = 'C';
      if        (adc_control_value < current_controller_value) {
        s_display_buffer[3][12] = '<';
      } else if (adc_control_value > current_controller_value) {
        s_display_buffer[3][12] = '>';
      } else {
        s_display_buffer[3][12] = '=';
      }

      std::snprintf(buff, sizeof(buff), "%3u", current_controller_value);
      s_display_buffer[3][13] = buff[0];
      s_display_buffer[3][14] = buff[1];
      s_display_buffer[3][15] = buff[2];

      char value_display_text[5] = {};
      boolean exists = PRA32_U2_ControlPanel_calc_value_display(adc_control_target_2, current_controller_value, value_display_text);
      if (exists) {
        s_display_buffer[3][16] = '[';
        s_display_buffer[3][17] = value_display_text[0];
        s_display_buffer[3][18] = value_display_text[1];
        s_display_buffer[3][19] = value_display_text[2];
        s_display_buffer[3][20] = ']';
      } else {
        std::memset(&s_display_buffer[3][16], ' ', 5);
      }
    } else {
      std::memset(&s_display_buffer[3][11], ' ', 10);
    }
  }
#endif  // defined(PRA32_U2_USE_CONTROL_PANEL)
}

INLINE void PRA32_U2_ControlPanel_update_display(uint32_t loop_counter) {
  static_cast<void>(loop_counter);

#if defined(PRA32_U2_USE_CONTROL_PANEL)

#if defined(PRA32_U2_USE_CONTROL_PANEL_OLED_DISPLAY)
  static uint32_t s_display_draw_position_x = 0;
  static uint32_t s_display_draw_position_y = 0;
  static bool     s_display_draw_position_update = false;

  if ((loop_counter & 0x3F) == 0x00) {
    if (s_display_draw_position_update) {
      PRA32_U2_ControlPanel_draw_character(s_display_buffer[s_display_draw_position_y][s_display_draw_position_x]);
      s_display_draw_position_update = false;
      return;
    }

    ++s_display_draw_counter;

    if (s_display_draw_counter == (15 * 10)) {
      s_display_draw_counter = (11 * 10);
    }

    switch (s_display_draw_counter / 10) {
    case 0:
      s_display_draw_position_x = s_display_draw_counter % 10;
      s_display_draw_position_y = 1;
      break;
    case 1:
      s_display_draw_position_x = s_display_draw_counter % 10;
      s_display_draw_position_y = 2;
      break;
    case 2:
      s_display_draw_position_x = s_display_draw_counter % 10;
      s_display_draw_position_y = 5;
      break;
    case 3:
      s_display_draw_position_x = s_display_draw_counter % 10;
      s_display_draw_position_y = 6;
      break;
    case 4:
      s_display_draw_position_x = s_display_draw_counter % 10;
      s_display_draw_position_y = 7;
      break;
    case 5:
      s_display_draw_position_x = s_display_draw_counter % 10 + 11;
      s_display_draw_position_y = 5;
      break;
    case 6:
      s_display_draw_position_x = s_display_draw_counter % 10 + 11;
      s_display_draw_position_y = 6;
      break;
    case 7:
      s_display_draw_position_x = s_display_draw_counter % 10 + 11;
      s_display_draw_position_y = 7;
      break;
    case 8:
      s_display_draw_position_x = s_display_draw_counter % 10 + 11;
      s_display_draw_position_y = 1;
      break;
    case 9:
      s_display_draw_position_x = s_display_draw_counter % 10 + 11;
      s_display_draw_position_y = 2;
      break;
    case 10:
      s_display_draw_position_x = s_display_draw_counter % 10 + 11;
      s_display_draw_position_y = 3;
      break;
    case 11:
      s_display_draw_position_x = s_display_draw_counter % 10;
      s_display_draw_position_y = 7;
      break;
    case 12:
      s_display_draw_position_x = s_display_draw_counter % 10 + 11;
      s_display_draw_position_y = 7;
      break;
    case 13:
      s_display_draw_position_x = s_display_draw_counter % 10 + 11;
      s_display_draw_position_y = 3;
      break;
    case 14:
      s_display_draw_position_x = s_display_draw_counter % 10 + 11;
      s_display_draw_position_y = 0;
      break;
    }

    if ((s_display_draw_position_x == 0) || (s_display_draw_position_x == 11)) {
      s_display_draw_position_update = true;
    }

    if (s_display_draw_position_update) {
      PRA32_U2_ControlPanel_set_draw_position(s_display_draw_position_x, s_display_draw_position_y);
    } else {
      PRA32_U2_ControlPanel_draw_character(s_display_buffer[s_display_draw_position_y][s_display_draw_position_x]);
    }
  }
#endif  // defined(PRA32_U2_USE_CONTROL_PANEL_OLED_DISPLAY)

#if defined(PRA32_U2_USE_CONTROL_PANEL_ST7789_DISPLAY)
  if (s_st7789_redraw_requested) {
    PRA32_U2_UI_RenderFrame frame = {};
    PRA32_U2_ControlPanel_build_st7789_frame(frame);
    PRA32_U2_UI_RenderST7789_draw(frame);
    s_st7789_redraw_requested = false;
  }
#endif  // defined(PRA32_U2_USE_CONTROL_PANEL_ST7789_DISPLAY)

#endif  // defined(PRA32_U2_USE_CONTROL_PANEL)
}

/* INLINE */ void __not_in_flash_func(PRA32_U2_ControlPanel_on_control_change)(uint8_t control_number)
{
  static_cast<void>(control_number);

#if defined(PRA32_U2_USE_CONTROL_PANEL)
  if (s_adc_control_target[0] == control_number) {
    s_adc_control_catched[0] = false;
  }

  if (s_adc_control_target[1] == control_number) {
    s_adc_control_catched[1] = false;
  }

  if (s_adc_control_target[2] == control_number) {
    s_adc_control_catched[2] = false;
  }

#if defined(PRA32_U2_USE_CONTROL_PANEL_ST7789_DISPLAY)
  if (PRA32_U2_ControlPanel_st7789_target_is_visible(control_number) ||
      (control_number == PANEL_PLAY_MODE)) {
    PRA32_U2_ControlPanel_request_st7789_redraw();
  }
#endif  // defined(PRA32_U2_USE_CONTROL_PANEL_ST7789_DISPLAY)

  if ((control_number == PANEL_PLAY_PIT ) ||
      (control_number == PANEL_PLAY_VELO) ||
      (control_number == PANEL_SCALE    ) ||
      (control_number == PANEL_PIT_OFST ) ||
      (control_number == PANEL_TRANSPOSE)) {
    PRA32_U2_ControlPanel_update_pitch(false);
  } else if (control_number == PANEL_PLAY_MODE) {
    uint8_t controller_value = g_synth.current_controller_value(PANEL_PLAY_MODE);
    uint8_t index = ((controller_value * 2) + 127) / 254;

    if (s_play_mode != index) {
      s_play_mode = index;

      s_playing_status = PlayingStatus_Stop;
      s_display_buffer[0][20] = ' ';
      s_panel_play_note_gate = false;

      PRA32_U2_ControlPanel_update_pitch(false);
      PRA32_U2_ControlPanel_update_page();
    }
  } else if (control_number == SEQ_TEMPO      ) {
    uint32_t bpm = PRA32_U2_ControlPanel_calc_bpm(g_synth.current_controller_value(SEQ_TEMPO      ));
    s_seq_count_increment = bpm * 8192;
  } else if (control_number == SEQ_CLOCK_SRC  ) {
    uint32_t controller_value = g_synth.current_controller_value(SEQ_CLOCK_SRC  );
    uint32_t index = ((controller_value * 2) + 127) / 254;
    s_seq_clock_src_external = index;
  } else if (control_number == SEQ_GATE_TIME  ) {
    uint8_t controller_value = g_synth.current_controller_value(SEQ_GATE_TIME  );
    s_seq_gate_time = (((controller_value * 10) + 127) / 254) + 1;
  } else if (control_number == SEQ_STEP_NOTE  ) {
    uint8_t ary[3] = {24, 12, 6};
    uint8_t controller_value = g_synth.current_controller_value(SEQ_STEP_NOTE  );
    uint32_t index = ((controller_value * 4) + 127) / 254;
    s_seq_step_clock_candidate = ary[index];
  } else if (control_number == SEQ_NUM_STEPS  ) {
    int32_t last_step = g_synth.current_controller_value(SEQ_NUM_STEPS  );
    last_step = (last_step - 2) >> 2;
    if (last_step < 0) { last_step = 0; }
    s_seq_last_step = last_step;
  } else if (control_number == SEQ_MODE       ) {
    uint8_t controller_value = g_synth.current_controller_value(SEQ_MODE       );
    uint32_t index = ((controller_value * 4) + 127) / 254;
    s_seq_mode = index;
  } else if (control_number == SEQ_ON_STEPS   ) {
    s_seq_on_steps = g_synth.current_controller_value(SEQ_ON_STEPS   );
  } else if (control_number == SEQ_ACT_STEPS  ) {
    s_seq_act_steps = g_synth.current_controller_value(SEQ_ACT_STEPS  );
  } else if (control_number == PANEL_MIDI_CH  ) {
    uint8_t midi_ch = g_synth.current_controller_value(PANEL_MIDI_CH  ) >> 3;
    g_midi_ch = midi_ch;
  }

#endif  // defined(PRA32_U2_USE_CONTROL_PANEL_ANALOG_INPUT)
}

INLINE void PRA32_U2_ControlPanel_debug_print(uint32_t loop_counter) {
  static_cast<void>(loop_counter);

#if defined(PRA32_U2_USE_DEBUG_PRINT)
#if defined(PRA32_U2_USE_CONTROL_PANEL)
  switch (loop_counter) {
  case  5 * 400:
    Serial1.print("\e[7;1H\e[K");
    Serial1.print(static_cast<char*>(s_display_buffer[0]));
    break;
  case  6 * 400:
    Serial1.print("\e[8;1H\e[K");
    Serial1.print(static_cast<char*>(s_display_buffer[1]));
    break;
  case  7 * 400:
    Serial1.print("\e[9;1H\e[K");
    Serial1.print(static_cast<char*>(s_display_buffer[2]));
    break;
  case  8 * 400:
    Serial1.print("\e[10;1H\e[K");
    Serial1.print(static_cast<char*>(s_display_buffer[3]));
    break;
  case  9 * 400:
    Serial1.print("\e[11;1H\e[K");
    Serial1.print(static_cast<char*>(s_display_buffer[4]));
    break;
  case 10 * 400:
    Serial1.print("\e[12;1H\e[K");
    Serial1.print(static_cast<char*>(s_display_buffer[5]));
    break;
  case 11 * 400:
    Serial1.print("\e[13;1H\e[K");
    Serial1.print(static_cast<char*>(s_display_buffer[6]));
    break;
  case 12 * 400:
    Serial1.print("\e[14;1H\e[K");
    Serial1.print(static_cast<char*>(s_display_buffer[7]));
    break;
#if defined(PRA32_U2_USE_CONTROL_PANEL_ANALOG_INPUT)
  case 13 * 400:
    Serial1.print("\e[16;1H\e[K");
    Serial1.print(s_adc_current_value[0]);
    break;
  case 14 * 400:
    Serial1.print("\e[17;1H\e[K");
    Serial1.print(s_adc_current_value[1]);
    break;
  case 15 * 400:
    Serial1.print("\e[18;1H\e[K");
    Serial1.print(s_adc_current_value[2]);
    break;
#endif  // defined(PRA32_U2_USE_CONTROL_PANEL_ANALOG_INPUT)
  }
#endif  // defined(PRA32_U2_USE_CONTROL_PANEL)
#endif  // defined(PRA32_U2_USE_DEBUG_PRINT)
}
