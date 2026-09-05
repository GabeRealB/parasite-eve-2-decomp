#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "main/display.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/text.h"
#include "main/ui.h"

extern s32      D_map_akropolis_8017A9A8;
extern s32      D_map_akropolis_8017A9AC[4];
extern TaskDesc D_map_akropolis_8017AA00;

extern UiObjectDesc D_8010EFA0;
extern UiObjectDesc D_map_akropolis_8017A9E4;

/// Draws one row of the Akropolis map's key-item list: the item's name at the
/// row's position, previewed while the row is highlighted. Confirming on the
/// selected row opens the item-detail panel `D_8010EFA0`; picking the row whose
/// item is 0x10C also records that choice in `D_map_akropolis_8017A9A8`, which
/// `func_map_akropolis_8017A038` reports back to the caller.
void func_map_akropolis_80179C50(DialogPrompt* arg0, UiObject* arg1)
{
    s32 item;
    s32 sel;

    item = D_map_akropolis_8017A9AC[arg0->field_8];
    Text_DrawPrompt(arg1, arg0->field_18, arg0->field_1A, (u8*)Gp_GetItemText(item, 0, 0), arg0->field_1C, 1, 0);
    if (((arg1->status >> 16) == 1) || (arg1->status == 1)) {
        if (arg0->field_10 == arg0->field_8) {
            Gp_SetPreviewItem(item, 0);
        }
    }
    sel = arg0->field_C;
    if (sel == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            Ui_SpawnFromDesc(&D_8010EFA0, item, 1, 1, arg1);
            arg1->status = 0;
            if (item == 0x10C) {
                D_map_akropolis_8017A9A8 = sel;
            }
        }
    }
}

INCLUDE_RODATA("mapui/nonmatchings/map_akropolis/map_akropolis", D_map_akropolis_80179950);

INCLUDE_ASM("mapui/nonmatchings/map_akropolis/map_akropolis", func_map_akropolis_80179D78);

/// Task driving the Akropolis key-item map screen: spawns the UI tree
/// `D_map_akropolis_8017A9E4`, freezes the game's frame timing while it is up,
/// then tears it down and releases the screen once the tree reports -1 or 6.
void func_map_akropolis_80179E8C(Task* task)
{
    UiObject* obj;
    s16       result;

    if (task->state == 0) {
        Display_InitPrimBufOnce();
        Gp_ClearPreviewItems();
        obj = Ui_SpawnFromDesc(&D_map_akropolis_8017A9E4, task->spawnArg1, 1, 1, NULL);
        if (obj == NULL) {
            return;
        }
        GameMain_SetFrameTiming(0);
        Game_Session->field_2 = 1;
        task->spawnArg2       = obj;
        task->state          += 1;
    }

    if (task->state == 1) {
        obj    = task->spawnArg2;
        result = obj->field_2E;
        if ((result == -1) || (result == 6)) {
            Ui_TeardownTree(obj, obj->owner);
            task->killCountdown = 0xA;
            task->state         = 2;
        }
    }

    if (task->state == 2) {
        task->killCountdown -= 1;
        if (task->killCountdown <= 0) {
            GameMain_SetFrameTiming(1);
            Game_Session->field_2 = 0;
            Task_Kill(task);
            Stage_ReleasePrimBuf();
            Stage_SetEndingFlag();
        }
    }
}

s32 func_map_akropolis_80179FC8(s32 arg0, s32 arg1)
{
    s32* p;
    s32  i;

    if (arg1 == 0) {
        i = 0;
        p = D_map_akropolis_8017A9AC;
        do {
            Gp_SetCollectedBit(*p++);
            i++;
        } while (i < 4);
        D_map_akropolis_8017A9A8 = 0;
    }
    Display_InitModeObj(&D_map_akropolis_8017AA00, 0, 0, 0);
    return 1;
}

s32 func_map_akropolis_8017A038(void)
{
    return D_map_akropolis_8017A9A8;
}

INCLUDE_RODATA("mapui/nonmatchings/map_akropolis/map_akropolis", D_map_akropolis_801799C0);

INCLUDE_RODATA("mapui/nonmatchings/map_akropolis/map_akropolis", D_map_akropolis_80179A34);

INCLUDE_RODATA("mapui/nonmatchings/map_akropolis/map_akropolis", D_map_akropolis_80179AC4);

INCLUDE_RODATA("mapui/nonmatchings/map_akropolis/map_akropolis", D_map_akropolis_80179B28);

INCLUDE_RODATA("mapui/nonmatchings/map_akropolis/map_akropolis", D_map_akropolis_80179B9C);

INCLUDE_RODATA("mapui/nonmatchings/map_akropolis/map_akropolis", D_map_akropolis_80179C08);
