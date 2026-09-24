#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "main/display.h"
#include "main/fs.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b6_nursery.h"

extern u8           D_80071086;
extern UiObjectDesc D_800611E4;

/// Title of the play-data menu, its list, and the child-panel descriptor it
/// shares with the usage list.
extern char         D_shelter_b6_nursery_8017D610[];
extern UiList       D_shelter_b6_nursery_80184F08;
extern UiObjectDesc D_shelter_b6_nursery_80184F54;

/// Lines of the four confirm prompts below, and the panels two of them open.
extern u8           D_shelter_b6_nursery_80184CBC[];
extern u8           D_shelter_b6_nursery_80184CC4[];
extern UiObjectDesc D_shelter_b6_nursery_80184F70;
extern u8           D_shelter_b6_nursery_80184CD0[];
extern u8           D_shelter_b6_nursery_80184CDC[];
extern UiObjectDesc D_shelter_b6_nursery_80184F8C;

/// Play-data menu task: draws its title, lays out the list and spawns the
/// child panel on its first tick, then updates the list every tick and marks
/// the panel for closing on cancel.
void func_shelter_b6_nursery_8017EF7C(Task* task)
{
    UiObject* obj;
    UiList*   list;

    list          = &D_shelter_b6_nursery_80184F08;
    obj           = task->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, D_shelter_b6_nursery_8017D610);
    if (task->state == 0) {
        Ui_SpawnFromDesc(&D_shelter_b6_nursery_80184F54, 0, 0, 1, obj);
        Ui_LayoutListPanel(list, (UiPanel*)obj);
        obj->field_12 += 5;
        list->field_A  = 1;
        Ui_SetListScrollFlag(list, 1);
        task->state += 1;
    }
    Ui_UpdateListNoAnim(list, obj);
    if (obj->status == 1 && Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
        obj->field_2E = 6;
    }
}

/// Queues a gouraud rectangle one ordering-table slot in front of the panel,
/// at offset (`arg1`, `arg2`) from the panel origin, `arg3` wide and `arg4`
/// high. The left edge takes colour `arg5` and the right edge `arg6`. Nothing
/// is drawn when `arg5` is zero or the width is below 2.
void func_shelter_b6_nursery_8017F06C(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6)
{
    register s32 dx asm("v1");
    register s32 w asm("t3");
    POLY_G4*     prim;
    s16          x;
    s16          y;

    dx = arg1;
    w  = arg3;
    if ((arg5 != 0) && (w >= 2)) {
        prim           = (POLY_G4*)gGpuPrimCursor;
        x              = arg0->field_20 + dx + 1;
        prim->x2       = x;
        prim->x0       = x;
        y              = arg0->field_22;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 8);
        *(u32*)&prim->r0 = arg5;
        setcode(prim, 0x38);
        *(u32*)&prim->r2 = arg5;
        *(u32*)&prim->r3 = arg6;
        *(u32*)&prim->r1 = arg6;
        y                = y + arg2 + 1;
        x                = (u16)prim->x0 + w - 1;
        prim->y1         = y;
        prim->y0         = y;
        prim->x3         = x;
        prim->x1         = x;
        y                = y + arg4 - 1;
        prim->y3         = y;
        prim->y2         = y;
        addPrim(gGpuCurrentOt + (s16)arg0->field_14 + 1, prim);
    }
}

/// Confirm-prompt row: draws its line, and on confirm - once the CD is idle -
/// plays sound 0x16, opens the `D_800611E4` panel and hands state 1 to the
/// owner.
void func_shelter_b6_nursery_8017F170(DialogPrompt* prompt, UiObject* obj)
{
    s32 sel;

    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b6_nursery_80184CBC, prompt->field_1C, 1, 0);
    sel = prompt->field_C;
    if (sel == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0 && CdCmd_IsIdle() != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        D_80071086 = 0xFF;
        Ui_SpawnFromDesc(&D_800611E4, 1, 0, 0, obj);
        obj->status       = 0;
        obj->field_2E     = 6;
        obj->owner->state = sel;
    }
}

/// Confirm-prompt row: draws its line, and on confirm plays sound 0x16, opens
/// the `D_shelter_b6_nursery_80184F70` panel and puts the owner in state 2.
void func_shelter_b6_nursery_8017F254(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b6_nursery_80184CC4, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_shelter_b6_nursery_80184F70, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// Confirm-prompt row: as `func_shelter_b6_nursery_8017F254`, opening the
/// `D_shelter_b6_nursery_80184F8C` panel with argument 0.
void func_shelter_b6_nursery_8017F31C(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b6_nursery_80184CD0, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_shelter_b6_nursery_80184F8C, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// Confirm-prompt row: as `func_shelter_b6_nursery_8017F254`, opening the
/// `D_shelter_b6_nursery_80184F8C` panel with argument 1.
void func_shelter_b6_nursery_8017F3E4(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b6_nursery_80184CDC, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_shelter_b6_nursery_80184F8C, 1, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// Exit callback of the prompt-lines panel task: releases `Wip_UiHolder` if the
/// task's panel still holds it, then frees the panel and kills the task.
void func_shelter_b6_nursery_8017F4AC(Task* task)
{
    WipUiHolder* holder;

    holder = task->spawnArg2;
    if (Wip_UiHolder == holder) {
        Wip_UiHolder = NULL;
    }
    Ui_FreeAndKill(task);
}
