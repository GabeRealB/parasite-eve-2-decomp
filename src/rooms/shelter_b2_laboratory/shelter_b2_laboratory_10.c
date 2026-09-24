#include "common.h"

#include <psyq/libgte.h>

#include "main/display.h"
#include "main/fs.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "rooms/room_common.h"

extern u8           D_80071086;
extern UiObjectDesc D_800611E4;

/// Descriptor spawned under the play-data panel's list.
extern UiObjectDesc D_shelter_b2_laboratory_80182398;

/// Title ("Play Data") and list of the play-data panel.
extern char   D_shelter_b2_laboratory_8017D610[];
extern UiList D_shelter_b2_laboratory_8018234C;

/// Captions of the telephone menu's rows ("Save", "Play Data", "Weapon Data",
/// "PE Data") and the descriptors of the panels the last three open.
extern u8           D_shelter_b2_laboratory_80182100[];
extern u8           D_shelter_b2_laboratory_80182108[];
extern UiObjectDesc D_shelter_b2_laboratory_801823B4;
extern u8           D_shelter_b2_laboratory_80182114[];
extern u8           D_shelter_b2_laboratory_80182120[];
extern UiObjectDesc D_shelter_b2_laboratory_801823D0;

/// Task body of the play-data panel: on its first frame it spawns
/// `D_shelter_b2_laboratory_80182398` and lays out the list; every frame it
/// draws the title, updates the list and closes on cancel.
void func_shelter_b2_laboratory_8017EF6C(Task* task)
{
    UiObject* obj;
    UiList*   list;

    list          = &D_shelter_b2_laboratory_8018234C;
    obj           = task->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, D_shelter_b2_laboratory_8017D610);
    if (task->state == 0) {
        Ui_SpawnFromDesc(&D_shelter_b2_laboratory_80182398, 0, 0, 1, obj);
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
void func_shelter_b2_laboratory_8017F05C(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6)
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
void func_shelter_b2_laboratory_8017F160(DialogPrompt* prompt, UiObject* obj)
{
    s32 sel;

    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b2_laboratory_80182100, prompt->field_1C, 1, 0);
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
/// `D_shelter_b2_laboratory_801823B4` and moves the owning task to state 2.
void func_shelter_b2_laboratory_8017F244(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b2_laboratory_80182108, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_shelter_b2_laboratory_801823B4, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// The telephone menu's "Weapon Data" row: confirmed, it opens the usage panel
/// `D_shelter_b2_laboratory_801823D0` for weapons and moves the owning task to
/// state 2.
void func_shelter_b2_laboratory_8017F30C(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b2_laboratory_80182114, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_shelter_b2_laboratory_801823D0, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// The telephone menu's "PE Data" row: confirmed, it opens the usage panel
/// `D_shelter_b2_laboratory_801823D0` for Parasite Energy and moves the owning
/// task to state 2.
void func_shelter_b2_laboratory_8017F3D4(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b2_laboratory_80182120, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_shelter_b2_laboratory_801823D0, 1, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// Task exit callback for the save-prompt UI: if this task still owns
/// `Wip_UiHolder`, clear it, then free the spawned UI object and kill the task.
void func_shelter_b2_laboratory_8017F49C(Task* task)
{
    WipUiHolder* holder;

    holder = task->spawnArg2;
    if (Wip_UiHolder == holder) {
        Wip_UiHolder = NULL;
    }
    Ui_FreeAndKill(task);
}
