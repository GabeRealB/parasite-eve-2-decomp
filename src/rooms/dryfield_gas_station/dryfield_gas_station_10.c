#include "common.h"

#include <psyq/libgte.h>

#include "main/display.h"
#include "main/fs.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "rooms/dryfield_gas_station.h"
#include "rooms/room_common.h"

extern u8           D_80071086;
extern UiObjectDesc D_800611E4;

/// Title of the "Play Data" panel.
extern char D_dryfield_gas_station_8017D610[];

/// Prompt texts: "Save", "Play Data", "Weapon Data" and "PE Data".
extern u8 D_dryfield_gas_station_80181AF8[];
extern u8 D_dryfield_gas_station_80181B00[];
extern u8 D_dryfield_gas_station_80181B0C[];
extern u8 D_dryfield_gas_station_80181B18[];

/// The "Play Data" statistics list.
extern UiList D_dryfield_gas_station_80181D44;

/// UI descriptor of the help-line box the "Play Data" panel opens beside its
/// list.
extern UiObjectDesc D_dryfield_gas_station_80181D90;

/// UI descriptors the "Play Data" and usage prompts open.
extern UiObjectDesc D_dryfield_gas_station_80181DAC;
extern UiObjectDesc D_dryfield_gas_station_80181DC8;

/// Task body of the "Play Data" panel: lays out its statistics list on the
/// first tick, then updates it each tick and closes on cancel.
void func_dryfield_gas_station_8017EF48(Task* task)
{
    UiObject* obj;
    UiList*   list;

    list          = &D_dryfield_gas_station_80181D44;
    obj           = task->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, D_dryfield_gas_station_8017D610);
    if (task->state == 0) {
        Ui_SpawnFromDesc(&D_dryfield_gas_station_80181D90, 0, 0, 1, obj);
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

/// Queues a gouraud rectangle one OT slot past the panel's draw order, at
/// (`arg1`, `arg2`) from the panel origin and `arg3` by `arg4` in size; the
/// left edge takes colour `arg5` and the right `arg6`. Nothing is drawn for a
/// zero `arg5` or a width below 2.
void func_dryfield_gas_station_8017F038(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6)
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

/// Prompt row "Save": on confirm, once the CD queue is idle, opens the save
/// panel and moves the owning task to state 1.
void func_dryfield_gas_station_8017F13C(DialogPrompt* prompt, UiObject* obj)
{
    s32 sel;

    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_gas_station_80181AF8, prompt->field_1C, 1, 0);
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

/// Prompt row "Play Data": on confirm opens the statistics panel.
void func_dryfield_gas_station_8017F220(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_gas_station_80181B00, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_dryfield_gas_station_80181DAC, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// Prompt row "Weapon Data": on confirm opens the usage panel for weapons.
void func_dryfield_gas_station_8017F2E8(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_gas_station_80181B0C, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_dryfield_gas_station_80181DC8, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// Prompt row "PE Data": on confirm opens the usage panel for Parasite
/// Energy.
void func_dryfield_gas_station_8017F3B0(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_gas_station_80181B18, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_dryfield_gas_station_80181DC8, 1, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// Exit callback of the help-line box task: releases `Wip_UiHolder` if the
/// task owns it, then frees the task's UI object and kills it.
void func_dryfield_gas_station_8017F478(Task* task)
{
    WipUiHolder* holder;

    holder = task->spawnArg2;
    if (Wip_UiHolder == holder) {
        Wip_UiHolder = NULL;
    }
    Ui_FreeAndKill(task);
}
