#include "ui.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "mc.h"
#include "task.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/ui_types.h"

/// Answers published by memory-card prompt rows with a confirm outcome.
enum {
    MEMORY_CARD_MENU_ANSWER_SELECTED = 1, // Yes, OK or the single Cancel action
    MEMORY_CARD_MENU_ANSWER_NO       = -1
};

static const char McText_Select[];

static UiListRowCallback Mc_YesNoCallbacks[];

static UiList Mc_YesNoList;

static UiListRowCallback Mc_OkCallbacks[];

static UiList Mc_OkList;

static UiListRowCallback Mc_YesCallbacks[];

static UiList Mc_YesList;

static void _uiUpdateListWithCenteredCursor(UiList* list, UiPanel* panel);

static void _mcMenuYesRow(UiList* list, UiObject* object);

static void _mcMenuOkRow(UiList* list, UiObject* object);

static void _mcMenuCancelRow(UiList* list, UiObject* object);

static void _mcMenuNoRow(UiList* list, UiObject* object);

static void McMenu_InitByMode(Task* task);

static const char McText_Select[]      = "Select";
const char        McText_Time[]        = "TIME";
const char        McText_Clear[]       = "CLEAR";
const char        McText_Nightmare[]   = "Nightmare";
const char        McText_Scavenger[]   = "Scavenger";
const char        McText_Bounty[]      = "Bounty";
const char        McText_Replay[]      = "Replay";
const char        McText_OpenParen[]   = " (";
const char        McText_Exp[]         = "EXP";
const char        McText_Unavailable[] = "---";
const char        McText_Bp[]          = "BP";

static UiListRowCallback Mc_YesNoCallbacks[] = { _mcMenuYesRow, _mcMenuNoRow };
static UiList            Mc_YesNoList        = { Mc_YesNoCallbacks, 2, 2, 0, 0x0F };
static UiListRowCallback Mc_OkCallbacks[]    = { _mcMenuOkRow };
static UiList            Mc_OkList           = { Mc_OkCallbacks, 1, 1, 0, 0x0F };
static UiListRowCallback Mc_YesCallbacks[]   = { _mcMenuCancelRow };
static UiList            Mc_YesList          = { Mc_YesCallbacks, 1, 1, 0, 0x0F };
UiObjectDesc             Mc_PromptDesc[]     = {
    { 0, { 0, 0, 0x4B, 0x20 }, 0x10, 0, TASK_BODY_NONE, 0xC0, McMenu_InitByMode, 0 },
};

void taskNoopBank0Slot12(Task* unusedTask)
{
    // Preserve the otherwise unused sixteen-byte stack frame.
    char unusedStackFrame[0x10];
}

/// Updates a list, then eases and draws an additional cursor at its content center Y.
///
/// Uses `uiUpdateList`'s live list, callback and embedded `UiObject.panel`
/// contract. With active control, the additional cursor target is two pixels
/// inside the content's left edge at content-relative Y zero. Both cursor calls
/// use the shared retained position and require `uiEaseAndDrawCursor`'s drawing
/// resources and signed coordinate bounds.
static void _uiUpdateListWithCenteredCursor(UiList* list, UiPanel* panel)
{
    uiUpdateList(list, panel);
    if (panel->control.word == USER_INTERFACE_PANEL_ACTIVE) {
        uiEaseAndDrawCursor(panel, panel->contentLeft.signedValue + 2, 0);
    }
}

void McMenu_SelectList(Task* task)
{
    UiPanel* obj;
    UiList*  menu;

    obj  = task->spawnArg2.pointer;
    menu = &Mc_SaveSlotList;
    uiDrawPanelLabel(obj, McText_Select);
    if (task->state == 0) {
        uiInitList(menu, obj);
        menu->flags                               = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        menu->selectedItemIndex                   = 0;
        menu->firstVisibleItemIndex.unsignedValue = 0;
        uiSetListSystemCursorSound(menu, 1);
        task->state += 1;
    } else {
        uiUpdateList(menu, obj);
        if (obj->control.word == USER_INTERFACE_PANEL_ACTIVE) {
            uiEaseAndDrawCursor(obj, obj->contentLeft.signedValue + 2, 0);
        }
    }
}

void McMenu_ConfirmWithRender(UiList* list, UiObject* object)
{
    s16     var_v0;
    McWork* work;
    s8      slot;

    slot = list->currentItemIndex;
    work = object->owner->spawnArg1.pointer;
    Mc_DrawSlotDetails(object, work, slot, 0, list->rowTextY.signedValue + 7);
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
            object->result = USER_INTERFACE_RESULT_CONFIRM;
            var_v0         = (s8)(u8)list->currentItemIndex;
            goto block_5;
        }
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
            sndEvtRequestScriptStart(SOUND_SYSTEM_CANCEL, 0, 0);
            object->result = USER_INTERFACE_RESULT_CONFIRM;
            var_v0         = -1;
        block_5:
            object->resultValue = var_v0;
        }
    }
}

void McMenu_SelectListAlt(Task* task)
{
    UiPanel* obj;
    UiList*  menu;
    McWork*  work;
    s32      temp;

    obj  = task->spawnArg2.pointer;
    work = task->spawnArg1.pointer;
    menu = &Mc_LoadSlotList;
    uiDrawPanelLabel(obj, McText_Select);
    if (task->state == 0) {
        uiInitList(menu, obj);
        menu->flags                               = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        menu->selectedItemIndex                   = work->selectedSlot;
        temp                                      = (u8)menu->selectedItemIndex - menu->visibleRowCount.unsignedValue + 1;
        menu->firstVisibleItemIndex.unsignedValue = temp;
        if ((s8)temp < 0) {
            menu->firstVisibleItemIndex.unsignedValue = 0;
        }
        uiSetListSystemCursorSound(menu, 1);
        task->state += 1;
    } else {
        uiUpdateList(menu, obj);
        if (obj->control.word == USER_INTERFACE_PANEL_ACTIVE) {
            uiEaseAndDrawCursor(obj, obj->contentLeft.signedValue + 2, 0);
        }
    }
}

void McMenu_FileInformation(Task* task)
{
    void*   obj;
    McWork* work;
    UiList* menu;
    s32     slot;

    obj = task->spawnArg2.pointer;
    if (task->state == 0) {
        task->killCountdown     = (u16)task->spawnArg1.value;
        work                    = task->parent->spawnArg1.pointer;
        task->state            += 1;
        task->spawnArg1.pointer = work;
    }
    work = task->spawnArg1.pointer;
    uiDrawTitle(obj, "File Information");
    if (task->killCountdown == 1) {
        menu = &Mc_LoadSlotList;
    } else {
        menu = &Mc_SaveSlotList;
    }
    slot = menu->selectedItemIndex;
    Mc_DrawSlotDetails(obj, work, slot, 0, 0);
}

/// Plays a prompt's selection sound and publishes its answer for the parent.
///
/// Borrows a live UI object; `answer` narrows to its signed halfword result.
/// Publishing does not close the child: the parent owns that transition.
static inline void _mcMenuPublishAnswer(UiObject* object, s32 answer, s32 soundScriptId)
{
    sndEvtRequestScriptStart(soundScriptId, 0, 0);
    object->result      = USER_INTERFACE_RESULT_CONFIRM;
    object->resultValue = answer;
}

/// Draws Yes and handles its selected row in the memory-card Yes/No prompt.
///
/// Uses the borrowed `UiListRowCallback` row coordinates, color and lifetime
/// contract. Active-row Confirm publishes answer 1 with the confirm sound;
/// Cancel plays the cursor sound and requests a step to the following No row.
/// Controller port zero is queried for newly pressed buttons; Confirm wins
/// when both buttons are pressed. The parent closes the prompt after an answer.
static void _mcMenuYesRow(UiList* list, UiObject* object)
{
    textDrawUiLine(object, list->rowTextX.signedValue, list->rowTextY.signedValue, McText_Yes, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            _mcMenuPublishAnswer(object, MEMORY_CARD_MENU_ANSWER_SELECTED, SOUND_SYSTEM_CONFIRM);
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
            // Cancel moves focus to No; a later input on that row publishes the answer.
            sndEvtRequestScriptStart(SOUND_SYSTEM_CURSOR, 0, 0);
            list->navigationStep = USER_INTERFACE_LIST_STEP_NEXT;
            list->actionResult   = USER_INTERFACE_LIST_ACTION_SKIP_ROW;
        }
    }
}

/// Draws OK and acknowledges the selected memory-card notice with answer 1.
///
/// Uses `UiListRowCallback`'s borrowed row coordinates, color and lifetime.
/// Only active-row Confirm on controller port zero publishes a confirm outcome
/// and plays the confirm sound. Cancel is ignored; the parent closes the notice.
static void _mcMenuOkRow(UiList* list, UiObject* object)
{
    textDrawUiLine(object, list->rowTextX.signedValue, list->rowTextY.signedValue, McText_Ok, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            _mcMenuPublishAnswer(object, MEMORY_CARD_MENU_ANSWER_SELECTED, SOUND_SYSTEM_CONFIRM);
        }
    }
}

/// Draws the single Cancel action in a memory-card prompt and selects it with answer 1.
///
/// Uses `UiListRowCallback`'s borrowed row coordinates, color and lifetime.
/// Only active-row Confirm on controller port zero publishes a confirm outcome
/// and plays the cancel sound. The Cancel button is ignored; the parent handles
/// the selected action and closes the prompt.
static void _mcMenuCancelRow(UiList* list, UiObject* object)
{
    textDrawUiLine(object, list->rowTextX.signedValue, list->rowTextY.signedValue, McText_Cancel, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            _mcMenuPublishAnswer(object, MEMORY_CARD_MENU_ANSWER_SELECTED, SOUND_SYSTEM_CANCEL);
        }
    }
}

/// Draws No and rejects the selected memory-card Yes/No prompt with answer -1.
///
/// Uses `UiListRowCallback`'s borrowed row coordinates, color and lifetime.
/// Active-row Confirm or Cancel on controller port zero plays the cancel sound
/// and publishes a confirm outcome carrying -1. The parent closes the prompt.
static void _mcMenuNoRow(UiList* list, UiObject* object)
{
    textDrawUiLine(object, list->rowTextX.signedValue, list->rowTextY.signedValue, McText_No, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
            _mcMenuPublishAnswer(object, MEMORY_CARD_MENU_ANSWER_NO, SOUND_SYSTEM_CANCEL);
        }
    }
}

static void McMenu_InitByMode(Task* task)
{
    UiPanel* obj;
    UiList*  menu;
    s32      mode;

    mode = task->spawnArg1.value;
    obj  = task->spawnArg2.pointer;
    if (mode == 2) {
        goto block_2;
    }
    if (mode >= 3) {
        goto block_default;
    }
    if (mode != 1) {
        goto block_default;
    }
    menu = &Mc_OkList;
    goto block_done;
block_2:
    menu = &Mc_YesList;
    goto block_done;
block_default:
    menu = &Mc_YesNoList;
block_done:
    if (task->state == 0) {
        uiFitPanelToList(menu, obj);
        obj->bounds.rect.y -= obj->bounds.rect.h / 2;
        if (task->spawnArg1.value != 3) {
            menu->selectedItemIndex = 0;
        } else {
            menu->selectedItemIndex = 1;
        }
        menu->firstVisibleItemIndex.unsignedValue = 0;
        uiSetListSystemCursorSound(menu, 1);
        task->state += 1;
    } else {
        uiUpdateList(menu, obj);
    }
}
