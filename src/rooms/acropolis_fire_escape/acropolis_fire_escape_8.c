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
#include "rooms/acropolis_fire_escape.h"

extern u8           D_80071086;
extern UiObjectDesc D_800611E4;

/// Title of the play-data panel.
extern char D_acropolis_fire_escape_8017D610[];

/// Captions of the telephone menu's rows ("Save", "Play Data", "Weapon Data",
/// "PE Data").
extern u8 D_acropolis_fire_escape_801819F8[];
extern u8 D_acropolis_fire_escape_80181A00[];
extern u8 D_acropolis_fire_escape_80181A0C[];
extern u8 D_acropolis_fire_escape_80181A18[];

/// List of the play-data panel; the row descriptor it spawns; and the
/// play-data and usage panels the telephone menu's rows open.
extern UiList       D_acropolis_fire_escape_80181C44;
extern UiObjectDesc D_acropolis_fire_escape_80181C90;
extern UiObjectDesc D_acropolis_fire_escape_80181CAC;
extern UiObjectDesc D_acropolis_fire_escape_80181CC8;

/// Task body of the play-data panel: on its first frame it spawns
/// `D_acropolis_fire_escape_80181C90` and lays out the list; every frame it
/// draws the title, updates the list and closes on cancel.
void func_acropolis_fire_escape_8017EF20(Task* task)
{
    UiObject* obj;
    UiList*   list;

    list          = &D_acropolis_fire_escape_80181C44;
    obj           = task->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, D_acropolis_fire_escape_8017D610);
    if (task->state == 0) {
        Ui_SpawnFromDesc(&D_acropolis_fire_escape_80181C90, 0, 0, 1, obj);
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

/// Queues a gouraud-shaded rectangle into the current OT one slot past the
/// panel's draw order. Origin is `field_20`/`field_22` plus (`arg1`, `arg2`);
/// `arg3`/`arg4` are width and height. Left vertices take `arg5`, right vertices
/// take `arg6`. A zero color or width < 2 draws nothing.
void func_acropolis_fire_escape_8017F010(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6)
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

/// The telephone menu's "Save" row: confirmed while the CD is idle, it spawns
/// `D_800611E4` and moves the owning task to state 1.
void func_acropolis_fire_escape_8017F114(DialogPrompt* prompt, UiObject* obj)
{
    s32 sel;

    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_acropolis_fire_escape_801819F8, prompt->field_1C, 1, 0);
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

/// The telephone menu's "Play Data" row: confirmed, it opens
/// `D_acropolis_fire_escape_80181CAC` and moves the owning task to state 2.
void func_acropolis_fire_escape_8017F1F8(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_acropolis_fire_escape_80181A00, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_acropolis_fire_escape_80181CAC, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// The telephone menu's "Weapon Data" row: confirmed, it opens the usage panel
/// `D_acropolis_fire_escape_80181CC8` for weapons and moves the owning task to
/// state 2.
void func_acropolis_fire_escape_8017F2C0(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_acropolis_fire_escape_80181A0C, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_acropolis_fire_escape_80181CC8, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// The telephone menu's "PE Data" row: confirmed, it opens the usage panel
/// `D_acropolis_fire_escape_80181CC8` for Parasite Energy and moves the owning
/// task to state 2.
void func_acropolis_fire_escape_8017F388(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_acropolis_fire_escape_80181A18, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_acropolis_fire_escape_80181CC8, 1, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// Task exit callback for the save-prompt UI: if this task still owns
/// `Wip_UiHolder`, clear it, then free the spawned UI object and kill the task.
void func_acropolis_fire_escape_8017F450(Task* task)
{
    WipUiHolder* holder;

    holder = task->spawnArg2;
    if (Wip_UiHolder == holder) {
        Wip_UiHolder = NULL;
    }
    Ui_FreeAndKill(task);
}
