#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/text.h"
#include "main/ui.h"

extern char     D_map_akropolis_8017997C[];
extern s32      D_map_akropolis_8017A9A8;
extern s32      D_map_akropolis_8017A9AC[4];
extern UiList   D_map_akropolis_8017A9C0;
extern TaskDesc D_map_akropolis_8017AA00;

extern UiObjectDesc D_8010EFA0;
extern UiObjectDesc D_map_akropolis_8017A9E4;

/// MDEC buffer layout hook for the Akropolis map, reached from
/// `Mdec_SetupBuffers` (main) for the stream kinds this overlay plays. Every
/// kind parks the two VLC buffers (`D_8006AC50`) and the two decode buffers
/// (`D_8006AC48`) around the frame allocation; they differ in how far apart
/// the halves sit — one frame (kinds 6/9/10), one and a half (11/14) — and in
/// whether they also resize the display. `D_8006AC44` always ends up one full
/// frame past the second decode buffer.
void func_map_akropolis_80179988(u8* arg0)
{
    CdCmdQueue* q = &CdCmd_Queue;
    s16         one;
    s32         strideA;
    s32         strideB;
    s32         halfA;
    s32         halfB;

    switch (arg0[2]) {
        case 5:
            q->field_22C  = 1;
            q->field_230  = 0x2C0;
            D_8006AC3C    = 0;
            q->field_232  = 0;
            D_8006AC50[0] = (u_long*)((u8*)D_8006AC60 + 0x10000);
            D_8006AC50[1] = (u_long*)D_8006AC40;
            D_8006AC48[1] = (u_long*)((u8*)D_8006AC40 + D_8006AC5A * D_8006AC6C);
            D_8006AC48[0] = D_8006AC48[1];
            break;
        case 8:
            /* The loop note pins `li 1` at the top of the block; without it the
               scheduler sinks it past the two display-size stores. */
            do {
                one = 1;
            } while (0);
            q->field_230           = 0x180;
            q->field_232           = 0x100;
            D_8006AC5C             = one;
            q->field_22C           = one;
            strideA                = D_8006AC5A * D_8006AC6C * 2;
            D_8006AC50[0]          = (u_long*)((u8*)D_8006AC60 + 0x10000);
            D_8006AC48[0]          = (u_long*)D_8006AC40;
            D_8006AC50[1]          = (u_long*)((u8*)D_8006AC50[0] + strideA);
            D_8006AC48[1]          = (u_long*)((u8*)D_8006AC48[0] + strideA);
            Game_Session->field_80 = 0;
            q->field_24A           = one;
            break;
        case 6:
        case 9:
        case 10:
            strideB       = D_8006AC5A * D_8006AC6C * 2;
            D_8006AC50[0] = (u_long*)((u8*)D_8006AC60 + 0x10000);
            D_8006AC50[1] = (u_long*)D_8006AC40;
            D_8006AC48[0] = (u_long*)((u8*)D_8006AC40 + strideB);
            D_8006AC48[1] = (u_long*)((u8*)D_8006AC48[0] + strideB);
            break;
        case 11:
            halfA         = D_8006AC5A * D_8006AC6C;
            D_8006AC50[0] = (u_long*)((u8*)D_8006AC60 + 0x10000);
            D_8006AC50[1] = (u_long*)D_8006AC40;
            D_8006AC48[0] = (u_long*)((u8*)D_8006AC40 + halfA * 3 / 2);
            D_8006AC48[1] = (u_long*)((u8*)D_8006AC48[0] + halfA * 2);
            break;
        case 14:
            halfB         = D_8006AC5A * D_8006AC6C;
            D_8006AC50[0] = (u_long*)((u8*)D_8006AC60 + 0x10000);
            D_8006AC48[0] = (u_long*)D_8006AC40;
            D_8006AC50[1] = (u_long*)((u8*)D_8006AC50[0] + halfB * 3 / 2);
            D_8006AC48[1] = (u_long*)((u8*)D_8006AC48[0] + halfB * 2);
            break;
    }
    D_8006AC44             = (u8*)D_8006AC48[1] + D_8006AC5A * D_8006AC6C * 2;
    Game_Session->field_7C = 0;
    Game_Session->field_7E = 0;
}

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

/// Per-frame handler for the Akropolis key-item map panel: draws the "Key Item"
/// heading, lays the key-item list out over the panel on the first frame, then
/// updates it. Cancel/menu asks the parent to close (field_2E = -1); once the
/// spawned item-detail child reports -1 or 6 the child tree is torn down and the
/// panel goes back to its active state.
void func_map_akropolis_80179D78(Task* task)
{
    UiObject* obj;
    UiList*   list;
    UiObject* child;
    s32       result;

    list          = &D_map_akropolis_8017A9C0;
    obj           = task->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, D_map_akropolis_8017997C);
    if (task->state == 0) {
        Ui_LayoutListPanel(list, (UiPanel*)obj);
        list->field_A   = 1;
        task->spawnArg1 = -1;
        task->state    += 1;
    }
    Ui_UpdateListNoAnim(list, obj);
    if (obj->status == 1 && Pad_CheckButtons(0, 1, Pad_MaskCancel | Pad_MaskMenu) != 0) {
        obj->field_2E = -1;
    }
    if (task->firstChild != NULL) {
        child  = task->firstChild->spawnArg2;
        result = child->field_2E;
        switch (result) {
            case 6:
                Ui_TeardownTree(child, child->owner);
                obj->status = 1;
                break;
            case -1:
                obj->field_2E = result;
                break;
        }
    }
}

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

INCLUDE_RODATA("mapui/nonmatchings/map_akropolis/map_akropolis", D_map_akropolis_8017997C);
