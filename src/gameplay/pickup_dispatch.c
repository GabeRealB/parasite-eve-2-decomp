#include "gameplay/item_menu.h"

#include "types.h"

#include "attachments.h"
#include "hud_sprites.h"
#include "item_menu.h"
#include "items.h"
#include "menu.h"
#include "gameplay/message.h"
#include "player_actor.h"
#include "gameplay/player_state.h"

#define D_8010EB94 D_8010EAB4[8]

#include "main/display.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys_types.h"

/// Five-entry dispatcher table: `Gp_PublishItemObj`, `Gp_SpawnPickupUiTask`, `Gp_PickupResultTask`,
/// `func_800CE188`, `Gp_PickupExitTask`. Copied onto the stack by `func_800CE22C`.
extern const TaskFuncTable5 D_80096E70;

static void func_800CE398(s32 arg0);

static s32 func_800CE3A4(void);

UiObjectTaskFunc D_8010D3A0[96] = {
    NULL,
    func_800CFA34,
    func_800CFA34,
    func_800CFA34,
    NULL,
    func_800CFA34,
    func_800CFA34,
    func_800CFA34,
    NULL,
    func_800CB6FC,
    func_800CB6FC,
    Gp_UiBoostMp,
    func_800CB6FC,
    Gp_UiBoostAttach,
    NULL,
    func_800CFAA8,
    func_800CFAA8,
    func_800CFAA8,
    func_800CFAA8,
    func_800CFAA8,
    func_800CFAA8,
    func_800CFAA8,
    func_800CFAA8,
    func_800CFAA8,
    func_800CFAA8,
    func_800CFAA8,
    func_800CFAA8,
    func_800CFAA8,
    func_800CFAA8,
    func_800CFAA8,
    func_800CFAA8,
    func_800CFAA8,
    func_800CFAA8,
    func_800CFAA8,
    func_800CFAA8,
    func_800CFAA8,
    func_800CFAA8,
    func_800CFAA8,
    func_800CFAA8,
    func_800CFAA8,
    func_800CFAA8,
    func_800CFAA8,
    func_800CFAA8,
    func_800CFAA8,
    func_800CFAA8,
    func_800CFAA8,
    func_800CFAA8,
    func_800CFAA8,
    func_800CFAA8,
    func_800CFAA8,
    func_800CFAA8,
    NULL,
    NULL,
    NULL,
    func_800CC41C,
    func_800CC41C,
    func_800CC41C,
    func_800CC41C,
    NULL,
    NULL,
    Gp_UiBoostHp,
    func_800CFA34,
    NULL,
    NULL,
    NULL,
    NULL,
    func_800CB6FC,
    func_800CB6FC,
    func_800CB6FC,
    func_800CB6FC,
    func_800CB6FC,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

const TaskFuncTable5 D_80096E70 = { { Gp_PublishItemObj, Gp_SpawnPickupUiTask, Gp_PickupResultTask, func_800CE188, Gp_PickupExitTask } };

// "EXP"
// "MP"

void func_800CE22C(Task* arg0)
{
    TaskFuncTable5 sp;

    sp = D_80096E70;
    sp.funcs[arg0->state](arg0);
}

void Gp_MenuExitCallback(Task* arg0)
{
    Task* playerTask;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if ((Gp_PendingRelatedId != 0) && (Gp_RelatedPending != 0)) {
        if (Gp_IsStateF0Active() == 0) {
            Gp_PendingRelatedId = 0;
        } else if (Gp_PendingRelatedId > 0) {
            func_801088D4(playerTask, 0, 1);
        } else {
            func_801088D4(playerTask, 1, 1);
        }
        Gp_RelatedPending = 0;
    }
    if (Gp_HealPending == 1) {
        taskMessageDispatch(playerTask, 0x402, 0, 0);
        Gp_HealPending = 0;
    }
    if (Gp_UsedItemId != 0) {
        if (Gp_UsedItemId == 0x3E) {
            Gp_TriggerPeState(0, PLAYER_STATUS_BERSERKER);
        }
        Gp_UsedItemId = 0;
    }
    Display_ReleaseRef();
    taskCallExit(arg0);
}

static void func_800CE398(s32 arg0)
{
    D_80114D88 = arg0;
}

static s32 func_800CE3A4(void)
{
    return D_80114D88;
}

void Gp_ItemMenuInit(UiObject* arg0, Task* arg1)
{
    void* mem;
    s32   scale;

    Wip_UiHolder = arg0;
    mem          = memCalloc(4, 0);
    if (mem != NULL) {
        arg1->work = mem;
        if (gGameSession->cutsceneHold == 1) {
            Gp_ClearPreviewItems();
            Ui_SpawnFromDesc(&D_8010EB94, 0, 1, 8, arg0);
            scale = 2;
        } else {
            Ui_SpawnFromDesc(&D_8010EAD0, 0, 1, 8, arg0);
            scale = 1;
        }
        Ui_UpdateLayoutSize(&(arg0)->panel, 0, Ui_Scale15(scale) + 1);
        arg0->panel.bounds.unsignedRect.y = 0x68 - arg0->panel.bounds.unsignedRect.h;
        arg1->state                       = arg1->state + 1;
    }
}

void Gp_ItemMenuTask(Task* arg0)
{
    UiObjectTaskFuncTable3 sp;

    sp = Gp_ItemMenuStates;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

void Gp_DrawPromptLines(UiObject* arg0, Task* arg1)
{
    const u8*    text;
    s32          color;
    s32          one;
    TaskSpawnArg val;

    val = arg1->spawnArg1;
    if (val.value != 0) {
        if (val.unsignedValue > 0xFFFF) {
            color = Ui_LookupTable(arg0, 1);
            one   = 1;
            Text_DrawPrompt(arg0, arg0->panel.contentLeft.signedValue + 2, arg0->panel.contentTop.signedValue + 0xF, val.pointer, color, one, TEXT_ALIGNMENT_LEFT);
            text = textSkipLines(val.pointer, one);
            Text_DrawPrompt(arg0, arg0->panel.contentLeft.signedValue + 2, arg0->panel.contentTop.signedValue + 0x1E, text, color, one, TEXT_ALIGNMENT_LEFT);
        } else if ((u32)(val.value - 0x300) < 0x100U) {
            Gp_DrawCastCostLines(arg0, val.value);
        }
    }
}

void func_800CE5D0(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    Gp_DrawItemIcon(arg0, arg1, arg2, arg3, 0);
}
