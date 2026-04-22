#include "pra32-u2-ui-state-machine.h"
#include "pra32-u2-ui-model.h"

static PRA32_U2_UI_State s_state = PRA32_U2_UI_State_GroupNavigation;
static PRA32_U2_UI_FocusItem s_items[3] = {};
static uint8_t s_item_count = 0;
static uint8_t s_item_index = 0;
static uint8_t s_edit_original_value = 0;
static bool s_confirm_selected = false;

static bool PRA32_U2_UI_StateMachine_is_dangerous_action(uint8_t target) {
  if ((target >= WR_PROGRAM_0 && target <= WR_PROGRAM_15) ||
      (target >= RD_PROGRAM_0 && target <= RD_PROGRAM_15)) {
    return true;
  }

  return (target == WR_PANEL_PRMS ||
          target == RD_PANEL_PRMS ||
          target == IN_PANEL_PRMS ||
          target == PANIC_OP ||
          target == SEQ_RAND_PITCH ||
          target == SEQ_RAND_VELO);
}

static PRA32_U2_UI_FocusItemType PRA32_U2_UI_StateMachine_item_type(uint8_t target) {
  if (target == 0xFF) {
    return PRA32_U2_UI_FocusItemType_None;
  }

  if (PRA32_U2_UI_StateMachine_is_dangerous_action(target)) {
    return PRA32_U2_UI_FocusItemType_Action;
  }

  return PRA32_U2_UI_FocusItemType_Parameter;
}

static void PRA32_U2_UI_StateMachine_rebuild_items() {
  s_item_count = 0;
  PRA32_U2_ControlPanelPage current_page = g_control_panel_page_table[s_current_page_group][s_current_page_index[s_current_page_group]];

  if (s_play_mode == 1) {
    current_page.control_target_c = SEQ_PIT_OFST;
  }

  const uint8_t targets[3] = {
    current_page.control_target_a,
    current_page.control_target_b,
    current_page.control_target_c,
  };

  for (uint8_t i = 0; i < 3; ++i) {
    PRA32_U2_UI_FocusItemType type = PRA32_U2_UI_StateMachine_item_type(targets[i]);
    if (type != PRA32_U2_UI_FocusItemType_None) {
      s_items[s_item_count].source_index = i;
      s_items[s_item_count].target = targets[i];
      s_items[s_item_count].type = type;
      ++s_item_count;
    }
  }

  if ((s_item_count > 0) && (s_item_index >= s_item_count)) {
    s_item_index = s_item_count - 1;
  }
  if (s_item_count == 0) {
    s_item_index = 0;
  }
}

static void PRA32_U2_UI_StateMachine_adjust_group(int8_t delta, PRA32_U2_UI_StateMachine_OnPageChangedFn on_page_changed) {
  if (delta == 0) {
    return;
  }

  int32_t group = static_cast<int32_t>(s_current_page_group) + delta;
  while (group < 0) {
    group += NUMBER_OF_PAGE_GROUPS;
  }
  while (group >= static_cast<int32_t>(NUMBER_OF_PAGE_GROUPS)) {
    group -= NUMBER_OF_PAGE_GROUPS;
  }

  s_current_page_group = static_cast<uint32_t>(group);
  if (s_current_page_index[s_current_page_group] >= g_number_of_pages[s_current_page_group]) {
    s_current_page_index[s_current_page_group] = g_number_of_pages[s_current_page_group] - 1;
  }
  on_page_changed();
  PRA32_U2_UI_StateMachine_rebuild_items();
}

static void PRA32_U2_UI_StateMachine_adjust_page(int8_t delta, PRA32_U2_UI_StateMachine_OnPageChangedFn on_page_changed) {
  if (delta == 0) {
    return;
  }

  int32_t page = static_cast<int32_t>(s_current_page_index[s_current_page_group]) + delta;
  uint32_t page_count = g_number_of_pages[s_current_page_group];
  while (page < 0) {
    page += page_count;
  }
  while (page >= static_cast<int32_t>(page_count)) {
    page -= page_count;
  }

  s_current_page_index[s_current_page_group] = static_cast<uint32_t>(page);
  on_page_changed();
  PRA32_U2_UI_StateMachine_rebuild_items();
}

void PRA32_U2_UI_StateMachine_initialize(PRA32_U2_UI_StateMachine_OnPageChangedFn on_page_changed) {
  s_state = PRA32_U2_UI_State_GroupNavigation;
  s_item_index = 0;
  s_confirm_selected = false;
  on_page_changed();
  PRA32_U2_UI_StateMachine_rebuild_items();
}

void PRA32_U2_UI_StateMachine_process_event(const PRA32_U2_UI_EncoderInputEvent& event,
                                            PRA32_U2_UI_StateMachine_GetValueFn get_value,
                                            PRA32_U2_UI_StateMachine_SetValueFn set_value,
                                            PRA32_U2_UI_StateMachine_ExecuteActionFn execute_action,
                                            PRA32_U2_UI_StateMachine_OnPageChangedFn on_page_changed) {
  if (event.rotation_delta == 0 && !event.short_click && !event.long_click) {
    return;
  }

  PRA32_U2_UI_StateMachine_rebuild_items();

  switch (s_state) {
  case PRA32_U2_UI_State_GroupNavigation:
    if (event.rotation_delta != 0) {
      PRA32_U2_UI_StateMachine_adjust_group(event.rotation_delta, on_page_changed);
    } else if (event.short_click) {
      s_state = PRA32_U2_UI_State_PageNavigation;
    }
    break;

  case PRA32_U2_UI_State_PageNavigation:
    if (event.rotation_delta != 0) {
      PRA32_U2_UI_StateMachine_adjust_page(event.rotation_delta, on_page_changed);
    } else if (event.short_click) {
      s_state = PRA32_U2_UI_State_ItemNavigation;
    } else if (event.long_click) {
      s_state = PRA32_U2_UI_State_GroupNavigation;
    }
    break;

  case PRA32_U2_UI_State_ItemNavigation:
    if ((event.rotation_delta != 0) && (s_item_count > 0)) {
      int32_t index = static_cast<int32_t>(s_item_index) + event.rotation_delta;
      while (index < 0) {
        index += s_item_count;
      }
      while (index >= s_item_count) {
        index -= s_item_count;
      }
      s_item_index = static_cast<uint8_t>(index);
    } else if (event.short_click && (s_item_count > 0)) {
      if (s_items[s_item_index].type == PRA32_U2_UI_FocusItemType_Action) {
        s_confirm_selected = false;
        s_state = PRA32_U2_UI_State_ActionConfirm;
      } else {
        s_edit_original_value = get_value(s_items[s_item_index].target);
        s_state = PRA32_U2_UI_State_ItemEdit;
      }
    } else if (event.long_click) {
      s_state = PRA32_U2_UI_State_PageNavigation;
    }
    break;

  case PRA32_U2_UI_State_ItemEdit:
    if ((event.rotation_delta != 0) && (s_item_count > 0)) {
      uint8_t target = s_items[s_item_index].target;
      int32_t value = static_cast<int32_t>(get_value(target)) + event.rotation_delta;
      if (value < 0) {
        value = 0;
      }
      if (value > 127) {
        value = 127;
      }
      set_value(target, static_cast<uint8_t>(value));
    } else if (event.short_click) {
      s_state = PRA32_U2_UI_State_ItemNavigation;
    } else if (event.long_click && (s_item_count > 0)) {
      set_value(s_items[s_item_index].target, s_edit_original_value);
      s_state = PRA32_U2_UI_State_ItemNavigation;
    }
    break;

  case PRA32_U2_UI_State_ActionConfirm:
    if (event.rotation_delta != 0) {
      s_confirm_selected = !s_confirm_selected;
    } else if (event.short_click) {
      if (s_confirm_selected && (s_item_count > 0)) {
        execute_action(s_items[s_item_index].target);
      }
      s_state = PRA32_U2_UI_State_ItemNavigation;
    } else if (event.long_click) {
      s_state = PRA32_U2_UI_State_ItemNavigation;
    }
    break;
  }
}

PRA32_U2_UI_StateSnapshot PRA32_U2_UI_StateMachine_snapshot() {
  PRA32_U2_UI_StateSnapshot snapshot = {
    s_state,
    s_item_index,
    s_item_count,
    s_confirm_selected,
  };
  return snapshot;
}

PRA32_U2_UI_FocusItem PRA32_U2_UI_StateMachine_focused_item() {
  PRA32_U2_UI_FocusItem item = {0xFF, 0xFF, PRA32_U2_UI_FocusItemType_None};
  if (s_item_count > 0) {
    item = s_items[s_item_index];
  }
  return item;
}
