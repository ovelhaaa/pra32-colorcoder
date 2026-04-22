#pragma once

#include "pra32-u2-common.h"
#include "pra32-u2-control-panel-page-table.h"
#include "pra32-u2-ui-input-encoder.h"

enum PRA32_U2_UI_State {
  PRA32_U2_UI_State_GroupNavigation = 0,
  PRA32_U2_UI_State_PageNavigation,
  PRA32_U2_UI_State_ItemNavigation,
  PRA32_U2_UI_State_ItemEdit,
  PRA32_U2_UI_State_ActionConfirm,
};

enum PRA32_U2_UI_FocusItemType {
  PRA32_U2_UI_FocusItemType_None = 0,
  PRA32_U2_UI_FocusItemType_Parameter,
  PRA32_U2_UI_FocusItemType_Action,
};

struct PRA32_U2_UI_FocusItem {
  uint8_t source_index;
  uint8_t target;
  PRA32_U2_UI_FocusItemType type;
};

struct PRA32_U2_UI_StateSnapshot {
  PRA32_U2_UI_State state;
  uint8_t focused_item_index;
  uint8_t focused_item_count;
  bool confirm_selected;
};

typedef uint8_t (*PRA32_U2_UI_StateMachine_GetValueFn)(uint8_t target);
typedef void (*PRA32_U2_UI_StateMachine_SetValueFn)(uint8_t target, uint8_t value);
typedef void (*PRA32_U2_UI_StateMachine_ExecuteActionFn)(uint8_t target);
typedef void (*PRA32_U2_UI_StateMachine_OnPageChangedFn)();

void PRA32_U2_UI_StateMachine_initialize(PRA32_U2_UI_StateMachine_OnPageChangedFn on_page_changed);
void PRA32_U2_UI_StateMachine_process_event(const PRA32_U2_UI_EncoderInputEvent& event,
                                            PRA32_U2_UI_StateMachine_GetValueFn get_value,
                                            PRA32_U2_UI_StateMachine_SetValueFn set_value,
                                            PRA32_U2_UI_StateMachine_ExecuteActionFn execute_action,
                                            PRA32_U2_UI_StateMachine_OnPageChangedFn on_page_changed);

PRA32_U2_UI_StateSnapshot PRA32_U2_UI_StateMachine_snapshot();
PRA32_U2_UI_FocusItem PRA32_U2_UI_StateMachine_focused_item();
bool PRA32_U2_UI_StateMachine_is_dangerous_action_target(uint8_t target);
