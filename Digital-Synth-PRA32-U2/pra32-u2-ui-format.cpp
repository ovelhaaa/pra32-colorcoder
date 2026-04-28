#include "pra32-u2-ui-format.h"

#include <cstdio>
#include <cstring>

extern uint8_t PRA32_U2_UI_get_current_controller_value(uint8_t control_number);
uint8_t PRA32_U2_ControlPanel_get_index_scale()
{
  uint8_t controller_value = PRA32_U2_UI_get_current_controller_value(PANEL_SCALE    );
  uint8_t index_scale = ((controller_value * 10) + 127) / 254;
  return index_scale;
}

uint8_t PRA32_U2_ControlPanel_calc_scaled_pitch(uint32_t index_scale, uint8_t pitch, int panel_pit_ofst, int seq_pit_ofst)
{
  if (pitch == 0) {
    return 0xFF;
  }

  if (pitch < 4) {
    pitch = 4;
  } else if (pitch > 124) {
    pitch = 124;
  }

  if (panel_pit_ofst < -60) {
    panel_pit_ofst = -60;
  } else if (panel_pit_ofst > +60) {
    panel_pit_ofst = +60;
  }

  if (seq_pit_ofst < -60) {
    seq_pit_ofst = -60;
  } else if (seq_pit_ofst > +60) {
    seq_pit_ofst = +60;
  }

  uint32_t new_pitch    = ((((pitch + panel_pit_ofst + seq_pit_ofst + 3) * 2) + 1) / 5) - 3 + 24;
  uint32_t index_pitch  = new_pitch % 24;
  uint32_t index_octave = new_pitch / 24;

  if        (index_scale == 0) {
    const uint8_t ary_major[25] =
      { 60, 60, 62, 62, 62, 62, 64, 64, 64, 65, 65, 65,
        67, 67, 67, 67, 69, 69, 69, 69, 71, 71, 71, 72, 72 };
    new_pitch  = ary_major[index_pitch];
    new_pitch += index_octave * 12 - 24;
  } else if (index_scale == 1) {
    const uint8_t ary_natural_minor[25] =
      { 60, 60, 62, 62, 62, 63, 63, 63, 65, 65, 65, 65,
        67, 67, 67, 68, 68, 68, 70, 70, 70, 70, 72, 72, 72 };
    new_pitch  = ary_natural_minor[index_pitch];
    new_pitch += index_octave * 12 - 24;
  } else if (index_scale == 2) {
    const uint8_t ary_melodic_minor[25] =
      { 60, 60, 62, 62, 62, 63, 63, 63, 65, 65, 65, 65,
        67, 67, 67, 67, 69, 69, 69, 69, 71, 71, 71, 72, 72 };
    new_pitch  = ary_melodic_minor[index_pitch];
    new_pitch += index_octave * 12 - 24;
  } else if (index_scale == 3) {
    const uint8_t ary_major_pentatonic[25] =
      { 60, 60, 62, 62, 62, 62, 64, 64, 64, 64, 64, 67,
        67, 67, 67, 67, 69, 69, 69, 69, 69, 72, 72, 72, 72 };
    new_pitch  = ary_major_pentatonic[index_pitch];
    new_pitch += index_octave * 12 - 24;
  } else if (index_scale == 4) {
    const uint8_t ary_minor_pentatonic[25] =
      { 60, 60, 60, 63, 63, 63, 63, 63, 65, 65, 65, 65,
        67, 67, 67, 67, 67, 70, 70, 70, 70, 70, 72, 72, 72 };
    new_pitch  = ary_minor_pentatonic[index_pitch];
    new_pitch += index_octave * 12 - 24;
  } else if (index_scale == 5) {
    const uint8_t ary_chromatic[25] =
      { 60, 61, 61, 62, 62, 63, 63, 64, 64, 65, 65, 66,
        66, 67, 67, 68, 68, 69, 69, 70, 70, 71, 71, 72, 72 };
    new_pitch  = ary_chromatic[index_pitch];
    new_pitch += index_octave * 12 - 24;
  }

  return new_pitch;
}

uint8_t PRA32_U2_ControlPanel_calc_transposed_pitch(uint8_t pitch, int32_t transpose)
{
  int32_t new_pitch = pitch + transpose;

  if (new_pitch < 0) {
    new_pitch = 0;
  } else if (new_pitch > 127) {
    new_pitch = 127;
  }

  return static_cast<uint8_t>(new_pitch);
}

void PRA32_U2_ControlPanel_calc_value_display_pitch(uint8_t pitch, char value_display_text[5])
{
  uint8_t index_scale = PRA32_U2_ControlPanel_get_index_scale();
  uint8_t new_pitch   = PRA32_U2_ControlPanel_calc_scaled_pitch(
                          index_scale, pitch, PRA32_U2_UI_get_current_controller_value(PANEL_PIT_OFST ) - 64,
                                              PRA32_U2_UI_get_current_controller_value(SEQ_PIT_OFST   ) - 64);

  if (new_pitch == 0xFF) {
    value_display_text[0] = 'O';
    value_display_text[1] = 'f';
    value_display_text[2] = 'f';
    value_display_text[3] = '\0';
    return;
  }

  new_pitch = PRA32_U2_ControlPanel_calc_transposed_pitch(
    new_pitch, PRA32_U2_UI_get_current_controller_value(PANEL_TRANSPOSE ) - 64);

  static const char ary[12][5] = { " C", "C#", " D", "D#", " E", " F", "F#", " G", "G#", " A", "A#", " B" };

  uint32_t quotient  = new_pitch / 12;
  uint32_t remainder = new_pitch % 12;

  value_display_text[0] = ary[remainder][0];
  value_display_text[1] = ary[remainder][1];

  if (quotient == 0) {
    value_display_text[2] = '-';
  } else {
    value_display_text[2] = '0' + quotient - 1;
  }
}

uint32_t PRA32_U2_ControlPanel_calc_bpm(uint8_t tempo_control_value) {
  uint32_t bpm = (tempo_control_value * 2) + 56;

  if (bpm > 300) {
    bpm = 300;
  }

  return bpm;
}

boolean PRA32_U2_ControlPanel_calc_value_display(uint8_t control_target, uint8_t controller_value, char value_display_text[5])
{
  boolean result = false;

  switch (control_target) {
  case OSC_2_COARSE    :
  case OSC_2_PITCH     :
  case FILTER_EG_AMT   :
  case FILTER_KEY_TRK  :
  case EG_OSC_AMT      :
  case LFO_OSC_AMT     :
  case LFO_FILTER_AMT  :
  case BTH_FILTER_AMT  :
  case PAN             :
  case PANEL_TRANSPOSE :
    {
      std::snprintf(value_display_text, 5, "%+3d", static_cast<int>(controller_value) - 64);
      result = true;
    }
    break;
  case PANEL_PIT_OFST  :
  case SEQ_PIT_OFST    :
    {
      int pit_ofst = controller_value - 64;
      if (pit_ofst < -60) {
        pit_ofst = -60;
      } else if (pit_ofst > +60) {
        pit_ofst = +60;
      }

      std::snprintf(value_display_text, 5, "%+3d", pit_ofst);
      result = true;
    }
    break;
  case MIXER_SUB_OSC   :
    {
      std::snprintf(value_display_text, 5, "%+3d", static_cast<int>(controller_value) - 64);

      if        (controller_value < 55) {
        value_display_text[0] = 'N';
      } else if (controller_value < 64) {
        value_display_text[1] = 'N';
      } else if (controller_value < 74) {
        value_display_text[1] = 'S';
      } else {
        value_display_text[0] = 'S';
      }

      result = true;
    }
    break;
  case OSC_1_WAVE      :
    {
      char ary[6][5] = {"Saw","Sqr","Tri","Sin"," WT","Pls"};
      uint32_t index = ((controller_value * 10) + 127) / 254;
      std::strcpy(value_display_text, ary[index]);
      result = true;
    }
    break;
  case OSC_2_WAVE      :
    {
      char ary[6][5] = {"Saw","Sqr","Tri","Sin"," O1","Nos"};
      uint32_t index = ((controller_value * 10) + 127) / 254;
      std::strcpy(value_display_text, ary[index]);
      result = true;
    }
    break;
  case EG_OSC_DST      :
  case LFO_OSC_DST     :
    {
      char ary[6][5] = {"  P","  P"," 2P"," 2P","  F"," 1S"};
      uint32_t index = ((controller_value * 10) + 127) / 254;
      std::strcpy(value_display_text, ary[index]);
      result = true;
    }
    break;
  case VOICE_MODE      :
    {
      char ary[6][5] = {"Pol","Pol","Mon","Mon"," LP","Lgt"};
      uint32_t index = ((controller_value * 10) + 127) / 254;
      std::strcpy(value_display_text, ary[index]);
      result = true;
    }
    break;
  case LFO_WAVE        :
    {
      char ary[6][5] = {"Tri","Sin","Nos","Saw","S&H","Sqr"};
      uint32_t index = ((controller_value * 10) + 127) / 254;
      std::strcpy(value_display_text, ary[index]);
      result = true;
    }
    break;
  case FILTER_MODE     :
    {
      char ary[2][5] = {" LP"," HP"};
      uint32_t index = ((controller_value * 2) + 127) / 254;
      std::strcpy(value_display_text, ary[index]);
      result = true;
    }
    break;
  case EG_AMP_MOD      :
  case REL_EQ_DECAY    :
  case SUSTAIN_PEDAL   :
  case SEQ_TRX_ST_SP   :
    {
      char ary[2][5] = {"Off"," On"};
      uint32_t index = ((controller_value * 2) + 127) / 254;
      std::strcpy(value_display_text, ary[index]);
      result = true;
    }
    break;
  case OSC_SAW_W_MODE  :
    {
      char ary[2][5] = {"Str","Cur"};
      uint32_t index = ((controller_value * 2) + 127) / 254;
      std::strcpy(value_display_text, ary[index]);
      result = true;
    }
    break;
  case BTH_AMP_MOD     :
    {
      char ary[3][5] = {"Off","Qad","Lin"};
      uint32_t index = ((controller_value * 4) + 127) / 254;
      std::strcpy(value_display_text, ary[index]);
      result = true;
    }
    break;
  case VOICE_ASGN_MODE :
    {
      char ary[2][5] = {"  1","  2"};
      uint32_t index = ((controller_value * 2) + 127) / 254;
      std::strcpy(value_display_text, ary[index]);
      result = true;
    }
    break;
  case DELAY_MODE      :
    {
      char ary[2][5] = {"  S","  P"};
      uint32_t index = ((controller_value * 2) + 127) / 254;
      std::strcpy(value_display_text, ary[index]);
      result = true;
    }
    break;
  case DELAY_TIME      :
    {
      int display_value;

      if (controller_value < 5) {
        display_value = controller_value + 1;
      } else if (controller_value < 27) {
        display_value = (controller_value * 2) -4;
      } else if (controller_value < 114) {
        display_value = (((controller_value * 20) + 3) / 6) - 40;
      } else {
        display_value = 340;
      }

      std::snprintf(value_display_text, 5, "%3d", display_value);
      result = true;
    }
    break;
  case  RD_PROGRAM_0   :
  case  RD_PROGRAM_1   :
  case  RD_PROGRAM_2   :
  case  RD_PROGRAM_3   :
  case  RD_PROGRAM_4   :
  case  RD_PROGRAM_5   :
  case  RD_PROGRAM_6   :
  case  RD_PROGRAM_7   :
  case  RD_PROGRAM_8   :
  case  RD_PROGRAM_9   :
  case  RD_PROGRAM_10  :
  case  RD_PROGRAM_11  :
  case  RD_PROGRAM_12  :
  case  RD_PROGRAM_13  :
  case  RD_PROGRAM_14  :
  case  RD_PROGRAM_15  :
  case  WR_PROGRAM_0   :
  case  WR_PROGRAM_1   :
  case  WR_PROGRAM_2   :
  case  WR_PROGRAM_3   :
  case  WR_PROGRAM_4   :
  case  WR_PROGRAM_5   :
  case  WR_PROGRAM_6   :
  case  WR_PROGRAM_7   :
  case  WR_PROGRAM_8   :
  case  WR_PROGRAM_9   :
  case  WR_PROGRAM_10  :
  case  WR_PROGRAM_11  :
  case  WR_PROGRAM_12  :
  case  WR_PROGRAM_13  :
  case  WR_PROGRAM_14  :
  case  WR_PROGRAM_15  :
  case  RD_PANEL_PRMS  :
  case  IN_PANEL_PRMS  :
  case  WR_PANEL_PRMS  :
  case  SEQ_RAND_PITCH :
  case  SEQ_RAND_VELO  :
  case  PANIC_OP       :
    {
      if        (controller_value <= 32) {
        value_display_text[0] = 'R';
        value_display_text[1] = 'd';
        value_display_text[2] = 'y';
      } else if (controller_value >= 96) {
        value_display_text[0] = 'E';
        value_display_text[1] = 'x';
        value_display_text[2] = 'e';
      } else {
        value_display_text[0] = ' ';
        value_display_text[1] = ' ';
        value_display_text[2] = '-';
      }

      result = true;
    }
    break;
  case  PANEL_PLAY_PIT :
  case  SEQ_PITCH_0    :
  case  SEQ_PITCH_1    :
  case  SEQ_PITCH_2    :
  case  SEQ_PITCH_3    :
  case  SEQ_PITCH_4    :
  case  SEQ_PITCH_5    :
  case  SEQ_PITCH_6    :
  case  SEQ_PITCH_7    :
    {
      PRA32_U2_ControlPanel_calc_value_display_pitch(controller_value, value_display_text);
      result = true;
    }
    break;
  case  PANEL_SCALE   :
    {
      char ary[6][5] = {"Maj","Min","Mel","MaP","MiP","Chr"};
      uint32_t index = PRA32_U2_ControlPanel_get_index_scale();
      std::strcpy(value_display_text, ary[index]);
      result = true;
    }
    break;
  case PANEL_PLAY_MODE:
    {
      char ary[2][5] = {"Nrm","Seq"};
      uint32_t index = ((controller_value * 2) + 127) / 254;
      std::strcpy(value_display_text, ary[index]);
      result = true;
    }
    break;
  case SEQ_TEMPO      :
    {
      uint32_t bpm = PRA32_U2_ControlPanel_calc_bpm(PRA32_U2_UI_get_current_controller_value(SEQ_TEMPO      ));
      std::snprintf(value_display_text, 5, "%3lu", bpm);
      result = true;
    }
    break;
  case SEQ_CLOCK_SRC  :
    {
      uint32_t index = ((controller_value * 2) + 127) / 254;

      if (index == 0) {
        value_display_text[0] = 'I';
        value_display_text[1] = 'n';
        value_display_text[2] = 't';
      } else {
        value_display_text[0] = 'E';
        value_display_text[1] = 'x';
        value_display_text[2] = 't';
      }

      result = true;
    }
    break;
  case SEQ_GATE_TIME  :
    {
      char ary[6][5] = {"1/6","2/6","3/6","4/6","5/6","6/6"};
      uint32_t index = ((controller_value * 10) + 127) / 254;
      std::strcpy(value_display_text, ary[index]);
      result = true;
    }
    break;
  case SEQ_STEP_NOTE   :
    {
      char ary[3][5] = {"  4","  8"," 16"};
      uint32_t index = ((controller_value * 4) + 127) / 254;
      std::strcpy(value_display_text, ary[index]);
      result = true;
    }
    break;
  case SEQ_NUM_STEPS  :
    {
      int32_t last_step = PRA32_U2_UI_get_current_controller_value(SEQ_NUM_STEPS  );
      last_step = (last_step - 2) >> 2;
      if (last_step < 0) { last_step = 0; }
      std::snprintf(value_display_text, 5, "%3ld", last_step + 1);
      result = true;
    }
    break;
  case SEQ_MODE       :
    {
      char ary[3][5] = {"Fwd","Rvs","Bnc"};
      uint32_t index = ((controller_value * 4) + 127) / 254;
      std::strcpy(value_display_text, ary[index]);
      result = true;
    }
    break;
  case SEQ_ON_STEPS   :
    {
      uint8_t on_steps = PRA32_U2_UI_get_current_controller_value(SEQ_ON_STEPS   );
      std::snprintf(value_display_text, 5, "x%02X", on_steps);
      result = true;
    }
    break;
  case SEQ_ACT_STEPS  :
    {
      uint8_t act_steps = PRA32_U2_UI_get_current_controller_value(SEQ_ACT_STEPS  );
      std::snprintf(value_display_text, 5, "x%02X", act_steps);
      result = true;
    }
    break;
  case PANEL_MIDI_CH  :
    {
      uint8_t midi_ch = PRA32_U2_UI_get_current_controller_value(PANEL_MIDI_CH  ) >> 3;
      std::snprintf(value_display_text, 5, "%3d", midi_ch + 1);
      result = true;
    }
    break;
  }

  return result;
}
