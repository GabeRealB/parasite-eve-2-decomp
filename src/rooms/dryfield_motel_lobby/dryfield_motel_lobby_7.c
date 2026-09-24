#include "common.h"

#include <psyq/libgte.h>

#include "main/display.h"
#include "main/fs.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "rooms/dryfield_motel_lobby.h"
#include "rooms/room_common.h"

extern u8           D_80071086;
extern UiObjectDesc D_800611E4;

/// Title of the "Play Data" panel `func_dryfield_motel_lobby_8017EEA0` draws.
extern char D_dryfield_motel_lobby_8017D610[];

/// The "Play Data" panel's row list.
extern UiList D_dryfield_motel_lobby_8017F73C;

/// Labels the four menu-entry handlers `func_dryfield_motel_lobby_8017F094` to
/// `func_dryfield_motel_lobby_8017F308` draw: "Save", "Play Data", "Weapon
/// Data" and "PE Data".
extern u8 D_dryfield_motel_lobby_8017F4F0[];
extern u8 D_dryfield_motel_lobby_8017F4F8[];
extern u8 D_dryfield_motel_lobby_8017F504[];
extern u8 D_dryfield_motel_lobby_8017F510[];

/// UI descriptors the "Play Data" entry and the two usage entries open.
extern UiObjectDesc D_dryfield_motel_lobby_8017F7A4;
extern UiObjectDesc D_dryfield_motel_lobby_8017F7C0;

void func_dryfield_motel_lobby_8017EEA0(Task* task)
{
    UiObject* obj;
    UiList*   list;

    list          = &D_dryfield_motel_lobby_8017F73C;
    obj           = task->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, D_dryfield_motel_lobby_8017D610);
    if (task->state == 0) {
        Ui_SpawnFromDesc(&D_dryfield_motel_lobby_8017F788, 0, 0, 1, obj);
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

/// Queues a gouraud rectangle into the current OT one slot past the panel's
/// draw order, at the panel origin (`field_20`, `field_22`) offset by (`arg1`,
/// `arg2`) and `arg3` by `arg4` in size. The left edge takes colour `arg5`, the
/// right edge `arg6`; nothing is drawn for a zero `arg5` or a width below 2.
void func_dryfield_motel_lobby_8017EF90(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6)
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

void func_dryfield_motel_lobby_8017F094(DialogPrompt* prompt, UiObject* obj)
{
    s32 sel;

    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_motel_lobby_8017F4F0, prompt->field_1C, 1, 0);
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

void func_dryfield_motel_lobby_8017F178(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_motel_lobby_8017F4F8, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_dryfield_motel_lobby_8017F7A4, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

void func_dryfield_motel_lobby_8017F240(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_motel_lobby_8017F504, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_dryfield_motel_lobby_8017F7C0, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

void func_dryfield_motel_lobby_8017F308(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_motel_lobby_8017F510, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_dryfield_motel_lobby_8017F7C0, 1, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

void func_dryfield_motel_lobby_8017F3D0(Task* task)
{
    WipUiHolder* holder;

    holder = task->spawnArg2;
    if (Wip_UiHolder == holder) {
        Wip_UiHolder = NULL;
    }
    Ui_FreeAndKill(task);
}
