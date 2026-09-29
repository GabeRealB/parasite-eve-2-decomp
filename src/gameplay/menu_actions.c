#include "gameplay/item_menu.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "gameplay/action_prompt.h"
#include "area_transitions.h"
#include "gameplay/attachment_state.h"
#include "attachment_state.h"
#include "gameplay/attachments.h"
#include "attachments.h"
#include "gameplay/captions.h"
#include "direction_input.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/inventory.h"
#include "item_menu.h"
#include "item_use.h"
#include "gameplay/items.h"
#include "items.h"
#include "loading.h"
#include "gameplay/map.h"
#include "gameplay/weapon_data.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/loadui.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"

#include "rooms/acropolis_east_elevator_hall.h"

#include "rooms/acropolis_square.h"

#include "rooms/acropolis_west_elevator_hall.h"

#include "rooms/dryfield_motel_room_6.h"

#include "rooms/dryfield_night_garage.h"

#include "rooms/dryfield_night_motel_room_6.h"

#include "rooms/dryfield_night_trailer_coach.h"

#include "rooms/dryfield_trailer_coach.h"

#include "rooms/mist_parking.h"

#include "rooms/shelter_1f_heliport.h"

#include "rooms/shelter_b1_armory.h"

#include "rooms/shelter_b1_underground_parking.h"

/// 0x10-byte scratch block `func_800D4270` carves off `G_SCRATCH_HEAD` for the
/// GTE round trip: `vx`/`vy`/`vz` receive the scaled vertex (`gte_stsv`) and
/// `offX`/`offY` are the screen-space offsets added to it.
typedef struct _GpMapMarkScratch {
    /* 0x0 */ u16 vx;
    /* 0x2 */ u16 vy;
    /* 0x4 */ u16 vz;
    /* 0x6 */ u16 pad_6;
    /* 0x8 */ u16 offX;
    /* 0xA */ u16 offY;
    /* 0xC */ u16 pad_C[2];
} GpMapMarkScratch;
STATIC_ASSERT_SIZEOF(GpMapMarkScratch, 0x10);

/// 0xC-byte scratchpad block `Gp_DrawMapIcons` carves off `G_SCRATCH_HEAD` to
/// stage one map icon position before it is turned into a `SPRT_16`.
typedef struct _GpMapIconPos {
    /* 0x0 */ u16 x;
    /* 0x2 */ u16 y;
    /* 0x4 */ u16 field_4;
    /* 0x6 */ u16 field_6;
    /* 0x8 */ u16 field_8;
    /* 0xA */ u16 field_A;
} GpMapIconPos;
STATIC_ASSERT_SIZEOF(GpMapIconPos, 0xC);

/// 0x1C-byte scratch block `Gp_DrawMapCursor` / `func_800D0614` carve off
/// `G_SCRATCH_HEAD` to stage the player cursor position on the map screen.
/// `x` / `y` are the map coordinates; only the tail from 0xC on is written.
typedef struct _GpMapCursorPos {
    /* 0x00 */ byte pad_0[0xC];
    /* 0x0C */ u16  x;
    /* 0x0E */ u16  y;
    /* 0x10 */ u16  field_10;
    /* 0x12 */ u16  field_12;
    /* 0x14 */ u16  field_14;
    /* 0x16 */ byte pad_16[6];
} GpMapCursorPos;
STATIC_ASSERT_SIZEOF(GpMapCursorPos, 0x1C);

RoomActionPrompt D_80114D28[2];

#define D_8010EBCC D_8010EAB4[10]

#define D_8010EE88 D_8010EAB4[35]

#define D_8010EFBC D_8010EAB4[46]

extern char D_8010F8F0[];

extern char Gp_StrReturnGame[];

extern char D_8010F91C[];

extern char Gp_StrUseAttachHelp[];

extern char Gp_StrUseKeyHelp[];

extern char Gp_StrCheckMap[];

static inline s32 _gpIsItemRowFree(McItemRec* arg0);

static McItemRec* func_800CE980(InventoryItemRange* arg0, s32 arg1);

static s32 func_800CEA00(InventoryItemRange* arg0, s32 arg1);

static s32 Gp_IsEquippedItem(s32 arg0);

static s32 func_800CEC5C(McItemRec* arg0);

static McItemRec* func_800CECC0(InventoryItemRange* arg0, s32 arg1);

static UiObject* Gp_OpenItemCmdMenu(UiList* arg0, UiObject* arg1, McItemRec* arg2, s32 arg3);

static void func_800CEE5C(UiObject* arg0);

static s32 func_800CF204(CdCmdEntry* arg0);

static s32 Gp_NthStockRelated(InventoryItemRange* arg0, s32 arg1, s32 arg2);

static void Gp_SpawnItemUsePrompt(UiList* arg0, UiObject* arg1);

static void Gp_DrawMapCursor(Task* arg0);

static void func_800D0614(Task* arg0);

static void Gp_DrawMapMarks(Task* arg0);

static void func_800D0C34(Task* arg0);

static s32 Gp_DrawMapIcons(Task* arg0, u8 arg1, u8 arg2);

static void Gp_EnqueueMapRoomCd(void);

static s8 func_800D1434(u32 roomId, u8 flagId);

static void func_800D15D0(Task* arg0);

static void func_800D1F90(Task* arg0);

static u8 Gp_GetMapRoomId(void);

static void func_800D2020(u8 arg0);

static void func_800D3660(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5);

static void func_800D3D98(UiObject* arg0, s32 arg1, s32 arg2);

static void func_800D4270(UiObject* obj, TmdSource* mesh, s32 mode, s32 dp);

static void Gp_DrawExamineCmd(UiObject* arg0, Task* arg1, u8* arg2, s32 arg3);

static void Gp_DrawPushCmd(UiObject* arg0, Task* arg1);

static inline s32 _gpIsItemRowFree(McItemRec* arg0)
{
    PlayerStatus* p;
    s32           ret;
    s32           id;
    s8            count;

    p     = &Player_Status;
    ret   = 1;
    count = arg0->attachSlot;
    id    = arg0->itemId;
    if ((count != 0) || (((u32)(id - 0x60) < 0x20U) && (p->armor == id - 0x5F)) ||
        (((u32)(id - 0x80) < 0x20U) && (p->weapon == id - 0x7F))) {
        ret = 0;
    }
    return ret;
}

char D_8010F8F0[]          = "Check game settings.";
char Gp_StrReturnGame[]    = "Return to the game.";
char D_8010F91C[]          = "Secret Debug menu";
char Gp_StrUseAttachHelp[] = "Use and attach items.";
char Gp_StrUseKeyHelp[]    = "Use key items.";
char Gp_StrCheckMap[]      = "Check the map.";

// "EXP"
// "MP"

void Gp_DrawPeEnergyCmd(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;
    s32         color;
    s32         status;
    s32         one;

    color = arg0->field_1C;
    if (Gp_IsDebugAttachRoom() != 0) {
        color = Ui_LookupTable(arg1, 2);
    } else {
        status = arg1->panel.field_0.w;
        one    = 1;
        if (((status >> 16) == one) || (status == one)) {
            if (arg0->field_10 == arg0->field_8) {
                Ui_SetHolderParam(Gp_StrReleasePe, 0, 0);
            }
        }
    }

    req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
    req.y          = arg1->panel.field_22.u + (u16)arg0->field_1A;
    req.otIndex    = arg1->panel.field_14.s + 1;
    req.field_8    = color;
    req.glyphTable = 5;
    req.centerMode = 0;
    req.field_E    = 0;
    Text_DrawString(&req, Gp_StrPEnergy);

    if (arg0->field_C == 1) {
        if (Gp_IsDebugAttachRoom() != 0) {
            arg0->field_22 = 0x41;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            arg1->field_2C = 0xC;
            arg1->field_2E = 6;
        }
    }
}

void Gp_DrawOptionCmd(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;
    s32         status;
    s32         one;
    s32         two;
    UiObject*   obj;

    req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
    req.y          = arg1->panel.field_22.u + (u16)arg0->field_1A;
    req.otIndex    = arg1->panel.field_14.s + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 5;
    req.centerMode = 0;
    req.field_E    = 0;
    Text_DrawString(&req, Gp_StrOption);

    status = arg1->panel.field_0.w;
    one    = 1;
    if (((status >> 16) == one) || (status == one)) {
        if (arg0->field_10 == arg0->field_8) {
            Ui_SetHolderParam(Gp_StrCustomizeHelp, 0, 0);
            two = 2;
            if (arg1->owner->spawnArg1.value != two) {
                CdCmd_DropPending();
                CdCmd_EnqueueLoadFile(1, 0, 0);
                Gp_ClearPreviewItems();
                arg1->owner->spawnArg1.value = two;
            }
        }
    }

    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            obj = (UiObject*)arg1->owner->spawnArg2.pointer;
            SndEvt_EnqueueType6(3, 0, 0);
            obj->field_2C = 0x24;
            obj->field_2E = 6;
        }
    }
}

void Gp_DrawExitCmd(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;
    s32         status;
    s32         one;

    req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
    req.y          = arg1->panel.field_22.u + (u16)arg0->field_1A;
    req.otIndex    = arg1->panel.field_14.s + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 5;
    req.centerMode = 0;
    req.field_E    = 0;
    Text_DrawString(&req, Gp_StrExit);

    status = arg1->panel.field_0.w;
    one    = 1;
    if (((status >> 16) == one) || (status == one)) {
        if (arg0->field_10 == arg0->field_8) {
            Ui_SetHolderParam(Gp_StrReturnGame, 0, 0);
        }
    }

    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            arg1->field_2E = -1;
        }
    }
}

static McItemRec* func_800CE980(InventoryItemRange* arg0, s32 arg1)
{
    McItemRec* table;
    s32        i;
    s32        count;
    McItemRec* rec;

    table = Gp_GetItemTable(arg0);
    i     = 0;
    rec   = NULL;
    table = &table[arg0->firstRow];
    count = arg0->rowCount;
    for (; i < count; i++) {
        if (table->attachSlot == arg1 + 1) {
            rec = table;
            break;
        }
        table++;
    }
    return rec;
}

static s32 func_800CEA00(InventoryItemRange* arg0, s32 arg1)
{
    McItemRec* table;
    s32        i;
    s32        count;
    McItemRec* rec;

    table = Gp_GetItemTable(arg0);
    i     = 0;
    rec   = NULL;
    table = &table[arg0->firstRow];
    count = arg0->rowCount;
    for (; i < count; i++) {
        if (table->attachSlot == arg1 + 1) {
            rec = table;
            break;
        }
        table++;
    }
    if (rec == NULL) {
        return 0;
    }
    return rec->itemId;
}

void Gp_WeaponSummaryTask(Task* arg0)
{
    UiObject*     obj;
    UiObjectDesc* desc;

    obj = arg0->spawnArg2.pointer;
    if (arg0->state == 0) {
        desc = &D_8010EBCC;
        Ui_SpawnFromDesc(desc, 0, 0, 0, obj);
        Ui_SpawnFromDesc(desc + 1, 0, 0, 0, obj);
        arg0->state = arg0->state + 1;
    }
    obj->field_2E = 0;
    Gp_DrawEquipSummary(&(obj)->panel, obj->panel.field_1C.s + 2, (s16)obj->panel.field_18.u + 0xF, 0);
    Ui_DrawTitle(&(obj)->panel, Gp_StrWeaponTitle);
}

void Gp_SetHolderItemText(s32 arg0)
{
    if (arg0 == 0) {
        Ui_SetHolderParam(Gp_StrEmpty, 0, 0);
    } else {
        Ui_SetHolderParam(Gp_GetItemText(arg0, 1, 0), 0, 0);
    }
}

static s32 Gp_IsEquippedItem(s32 arg0)
{
    s32           ret;
    PlayerStatus* p;

    ret = 0;
    p   = &Player_Status;
    if ((((u32)(arg0 - 0x80) < 0x20U) && (p->weapon == arg0 - 0x7F)) ||
        (((u32)(arg0 - 0x60) < 0x20U) && (p->armor == arg0 - 0x5F)) ||
        (((u32)(arg0 - 0xA0) < 0x20U) && (p->weapon != 0) &&
         ((Gp_GetItemSlot(p->weapon + 0x7F)->ammoId == arg0) ||
          (Gp_GetItemSlot(p->weapon + 0x7F)->attachId == arg0)))) {
        ret = 1;
    }
    return ret;
}

static s32 func_800CEC5C(McItemRec* arg0)
{
    return _gpIsItemRowFree(arg0);
}

static McItemRec* func_800CECC0(InventoryItemRange* arg0, s32 arg1)
{
    McItemRec* table;
    s32        i;
    McItemRec* rec;

    table = Gp_GetItemTable(arg0);
    rec   = NULL;
    table = &table[arg0->firstRow];
    for (i = 0; i < arg0->rowCount; i++, table++) {
        if (_gpIsItemRowFree(table) == 1) {
            arg1--;
        }
        if (arg1 < 0) {
            rec = table;
            break;
        }
    }
    return rec;
}

static UiObject* Gp_OpenItemCmdMenu(UiList* arg0, UiObject* arg1, McItemRec* arg2, s32 arg3)
{
    UiObject* obj;
    s32       one;

    obj           = NULL;
    Gp_SelItemRec = arg2;
    if (Pad_CheckButtons(0, 1, Pad_MaskConfirm)) {
        SndEvt_EnqueueType6(3, 0, 0);
        one = 1;
        obj = Ui_SpawnFromDesc(&D_8010EE6C, arg3, one, one, arg1);
        if (obj != NULL) {
            Ui_ClampDialogRect(&(obj)->panel, arg0, &(arg1)->panel);
            arg1->panel.field_0.w = 0;
        }
    } else {
        Gp_CheckItemInfoButton(arg1);
    }
    return obj;
}

static void func_800CEE5C(UiObject* arg0)
{
    Task*     owner;
    Task*     child;
    Task*     next;
    Task*     head;
    UiObject* obj;
    s32       one;
    s32       mask;
    s32       flag;

    owner = arg0->owner;
    head  = owner->firstChild;
    if (head != NULL) {
        one   = 1;
        child = head;
        mask  = 0xFFFEFFFF;
        do {
            obj  = child->spawnArg2.pointer;
            flag = obj->field_2E;
            next = child->nextSibling;
            switch (flag) {
                case -1:
                    arg0->field_2E = flag;
                    break;
                case 6:
                    Ui_TeardownTree(obj, obj->owner);
                    arg0->panel.field_0.w = one;
                    arg0->panel.field_4  &= mask;
                    break;
                case 0x23:
                    Ui_TeardownTree(obj, obj->owner);
                    arg0->panel.field_0.w = one;
                    Gp_ItemOrderMode      = one;
                    arg0->panel.field_4  &= mask;
                    break;
            }
            head  = owner->firstChild;
            child = next;
            if (child == head) {
                break;
            }
        } while (head != NULL);
    }
}

void Gp_DrawSortCmd(UiList* arg0, UiObject* arg1)
{
    s32 status;
    s32 one;

    one = 1;
    if (Gp_ItemOrderMode == one) {
        arg0->field_1C = Ui_LookupTable(arg1, 2);
    }
    Text_DrawPrompt(arg1, arg0->field_18, arg0->field_1A, Gp_StrSort, arg0->field_1C, one, 0);
    status = arg1->panel.field_0.w;
    if (((status >> 16) == one) || (status == one)) {
        if (arg0->field_10 == arg0->field_8) {
            if (Gp_ItemOrderMode == one) {
                arg0->field_22 = 0x41;
                arg0->field_C  = 0;
            } else {
                Ui_SetHolderParam(Gp_StrChangeOrderHelp, 0, 0);
            }
        }
    }
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            Gp_SortItems(&Mc_SaveData[0].state.carriedItems, 1);
        }
    }
}

void func_800CF090(UiList* arg0, UiObject* arg1)
{
    PlayerStatus*       p;
    InventoryItemRange* scan;
    volatile McItemRec* table;
    s32                 count;
    s32                 i;

    count = 0;
    p     = &Player_Status;
    scan  = &Mc_SaveData[0].state.carriedItems;
    table = Gp_GetItemTable(scan);
    i     = 0;
    table = &table[scan->firstRow];
    for (; i < scan->rowCount; i++) {
        if (((u32)(table->itemId - 0x60) < 0x20U) && (p->armor != table->itemId - 0x5F)) {
            count++;
        }
        table++;
    }
    arg0->field_4   = count;
    arg0->field_5.u = 4;
}

void func_800CF148(UiObject* arg0, Task* arg1)
{
    Task*     child;
    Task*     next;
    Task*     head;
    UiObject* childObj;
    s32       flag;
    s32       val;

    child = arg1->firstChild;
    if (child != NULL) {
        val = 6;
        do {
            childObj = child->spawnArg2.pointer;
            flag     = childObj->field_2E;
            next     = child->nextSibling;
            switch (flag) {
                case 9:
                    arg0->field_2E = flag;
                    break;
                case -1:
                    arg0->field_2E = flag;
                    break;
                case 6:
                    Ui_TeardownTree(childObj, childObj->owner);
                    arg0->panel.field_0.w = 1;
                    break;
            }
            head  = arg1->firstChild;
            child = next;
            if (child == head) {
                break;
            }
            if (head == NULL) {
                break;
            }
        } while (1);
    }
}

static s32 func_800CF204(CdCmdEntry* arg0)
{
    return cdCmdEnqueueEntry(arg0);
}

s32 Gp_GetPreviewItem(void)
{
    return Gp_PreviewItems[0];
}

void Gp_DrawItemDescLine(UiList* arg0, UiObject* arg1)
{
    u8* text;
    s8  idx;
    s32 id;

    idx = arg0->field_8;
    id  = (u16)arg1->owner->spawnArg1.value;
    if ((idx < 2) && (id < 0x100)) {
        text = Gp_GetItemText(id, idx + 1, 1);
        Text_DrawPrompt(arg1, arg0->field_18, arg0->field_1A, text, 0x606060, 3, 0);
    } else {
        text = Text_SkipLines(Fs_GetChunkPayload(), arg0->field_8 + 5);
        Text_DrawPrompt(arg1, arg0->field_18, arg0->field_1A, text, 0x606060, 3, 0);
    }
}

void func_800CF330(Task* arg0)
{
    UiObject* obj;

    obj = arg0->spawnArg2.pointer;
    if (obj != NULL) {
        if (D_80067634 == obj) {
            D_80067634 = NULL;
        }
    }
    Ui_FreeAndKill(arg0);
}

void Gp_DrawUseCmd(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;

    req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
    req.y          = arg1->panel.field_22.u + (u16)arg0->field_1A;
    req.otIndex    = arg1->panel.field_14.s + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    Text_DrawString(&req, Gp_StrUse);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            Ui_SpawnFromDesc(&D_8010EF84, 0, 1, 1, arg1);
            arg1->panel.field_0.w = 0;
        }
    }
}

void Gp_EquipHeld(s32 arg0)
{
    PlayerStatus* p;
    McItemRec*    rec;
    McItemRec*    prev;
    u8            field21;

    p       = &Player_Status;
    rec     = Gp_FindItemById(arg0);
    field21 = p->weapon;
    if (field21 != arg0 - 0x7F) {
        if (field21 != 0) {
            prev = Gp_FindItemById(field21 + 0x7F);
            if (rec->attachSlot > 0) {
                prev->attachSlot = rec->attachSlot;
            } else {
                Gp_ClearEquipSlotSel(prev->itemId, 0);
            }
        }
        p->weapon = arg0 - 0x7F;
        Gp_RefreshItemRow(rec);
        Gp_SetItemSeenBit(arg0, 1);
    }
}

static s32 Gp_NthStockRelated(InventoryItemRange* arg0, s32 arg1, s32 arg2)
{
    s32 i;
    s32 result;
    s32 item;
    s32 qty;
    s32 mode;
    s32 idx;
    s32 temp;
    u8* table0;
    u8* table1;

    result = 0;
    mode   = Gp_ReloadMode;
    if (mode != 2) {
        i      = 0;
        table0 = Gp_RelatedQty0.bytes;
        idx    = arg2 - 0x80;
        do {
            temp = i + idx * 4;
            item = table0[temp + 1];
            qty  = Gp_ScanStackQty(arg0, item);
            qty -= Gp_CountEquippedRelated(arg0, item);
            if (qty > 0) {
                arg1--;
                if (arg1 < 0) {
                    result = item;
                    break;
                }
            }
            i++;
        } while (i < 3);
    }
    if (mode != 1) {
        if (arg1 >= 0) {
            i      = 0;
            table1 = Gp_RelatedQty1.bytes;
            idx    = arg2 - 0x80;
            do {
                temp = i + idx * 4;
                item = table1[temp + 1];
                qty  = Gp_ScanStackQty(arg0, item);
                qty -= Gp_CountEquippedRelated(arg0, item);
                if (qty > 0) {
                    arg1--;
                    if (arg1 < 0) {
                        result = item;
                        break;
                    }
                }
                i++;
            } while (i < 3);
        }
    }
    return result;
}

void Gp_SizeEquippedPanel(UiPanel* arg0, s32 arg1)
{
    s32 width;
    s32 temp;

    width = Text_MeasureWidth(Gp_GetItemText(arg1, 0, 0)) + 0xB;
    temp  = Text_MeasureWidth(Gp_StrEquipped);
    if (width < temp) {
        width = temp;
    }
    Ui_UpdateLayoutSize(arg0, width + 5, Ui_Scale15(2) + 1);
    arg0->bounds.rect.x = (-arg0->bounds.rect.w) >> 1;
}

void func_800CF6E8(UiObject* arg0, s32 arg1)
{
    u8* text;
    s32 color;
    s32 one;
    s32 x;

    text  = Gp_GetItemText(arg1, 0, 0);
    color = 0x606060;
    one   = 1;
    Text_DrawPrompt(arg0, arg0->panel.field_1C.s + 2, (s16)arg0->panel.field_18.u + 0xF, Gp_StrEquipped, color, one, 0);
    x = Text_DrawPrompt(arg0, arg0->panel.field_1C.s + 2, (s16)arg0->panel.field_18.u + 0x1E, text, 0x37A78, one, 0);
    Text_DrawPrompt(arg0, x, (s16)arg0->panel.field_18.u + 0x1E, Gp_StrDot, color, one, 0);
}

void Gp_DrawUsePrompt(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;

    req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
    req.y          = arg1->panel.field_22.u + (u16)arg0->field_1A;
    req.otIndex    = arg1->panel.field_14.s + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    Text_DrawString(&req, Gp_StrUse);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            Gp_SpawnItemUsePrompt(arg0, arg1);
            arg0->field_22 = 0x20;
        }
    }
}

void Gp_DrawMovePrompt(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;

    req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
    req.y          = arg1->panel.field_22.u + (u16)arg0->field_1A;
    req.otIndex    = arg1->panel.field_14.s + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    Text_DrawString(&req, Gp_StrMove);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            arg0->field_22 = 0x23;
        }
    }
}

void Gp_DrawExchangeSlotCmd(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;
    UiObject*   obj;
    s32         one;
    s32         x;
    s32         y;

    req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
    req.y          = arg1->panel.field_22.u + (u16)arg0->field_1A;
    req.otIndex    = arg1->panel.field_14.s + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    Text_DrawString(&req, Gp_StrExchange);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            one = 1;
            obj = Ui_SpawnFromDesc(&D_8010ED00, one, one, 0x10, arg1);
            if (obj != NULL) {
                y                                = -0x5C;
                obj->panel.bounds.unsignedRect.y = y;
                x                                = -8;
                obj->panel.bounds.unsignedRect.x = x;
            }
            arg1->panel.field_0.w = 0;
            arg0->field_22        = 0x20;
        }
    }
}

void func_800CFA34(UiObject* arg0, Task* arg1)
{
    Gp_UseHealItemPanel(arg0, arg1, Gp_SelItemRec->itemId);
}

void func_800CFA60(Task* arg0)
{
    void      (*fn)(UiObject*, Task*);
    UiObject* obj;

    fn  = D_8010D3A0[arg0->spawnArg1.value];
    obj = arg0->spawnArg2.pointer;
    if (fn != NULL) {
        fn(obj, obj->owner);
    }
}

void func_800CFAA8(UiObject* arg0, Task* arg1)
{
    Gp_InvokePeItemPanel(arg0, arg1, arg1->spawnArg1.value);
}

void Gp_DrawOkCmd(UiList* arg0, UiObject* arg1)
{
    Text_DrawPrompt(arg1, arg0->field_18, arg0->field_1A, Gp_StrOk, arg0->field_1C, 1, 0);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            arg0->field_22   = 6;
            arg0->field_20.s = 0x36;
        }
    }
}

void Gp_DrawCancelCmd(UiList* arg0, UiObject* arg1)
{
    Text_DrawPrompt(arg1, arg0->field_18, arg0->field_1A, Gp_StrCancel, arg0->field_1C, 1, 0);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            arg0->field_22   = 6;
            arg0->field_20.s = 0x35;
        }
    }
}

void Gp_DrawYesCmd(UiList* arg0, UiObject* arg1)
{
    s32 temp;

    Text_DrawPrompt(arg1, arg0->field_18, arg0->field_1A, Gp_StrYes, arg0->field_1C, 1, 0);
    temp = arg0->field_C;
    if (temp == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            arg0->field_22   = 6;
            arg0->field_20.s = 0x33;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(2, 0, 0);
            arg0->field_B  = temp;
            arg0->field_22 = 0x41;
        }
    }
}

void Gp_DrawNoCmd(UiList* arg0, UiObject* arg1)
{
    Text_DrawPrompt(arg1, arg0->field_18, arg0->field_1A, Gp_StrNo, arg0->field_1C, 1, 0);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(4, 0, 0);
            arg0->field_22   = 6;
            arg0->field_20.s = 0x34;
        }
    }
}

void func_800CFD78(Task* arg0)
{
    if (arg0->state == 0) {
        D_80114DCC = GAME_LOCATION_WORD(gGameSession->at4.loc) & GAME_LOCATION_STAGE_AREA_MASK;
    }
    switch (D_80114DCC) {
        case 0x1010000:
            func_acropolis_square_8017F41C(arg0);
            break;
        case 0x1020000:
            func_acropolis_east_elevator_hall_8017F2F8(arg0);
            break;
        case 0x1110000:
            func_acropolis_west_elevator_hall_8017F304(arg0);
            break;
        case 0x21E0000:
            func_dryfield_motel_room_6_80181184(arg0);
            break;
        case 0x31E0000:
            func_dryfield_night_motel_room_6_801811A0(arg0);
            break;
        default:
            taskKill(arg0);
            break;
    }
}

static void Gp_SpawnItemUsePrompt(UiList* arg0, UiObject* arg1)
{
    u8   id;
    s32  one;
    void (**slot)(UiObject*, Task*);

    id   = Gp_SelItemRec->itemId;
    slot = &D_8010D3A0[id];
    if (*slot != NULL) {
        one = 1;
        if (Ui_SpawnFromDesc(&D_8010EE88, (s32)(id), one, one, arg1) != NULL) {
            Gp_SetItemSeenBit(id, 1);
        }
        arg1->panel.field_0.w = 0;
    } else {
        Gp_SpawnItemPrompt(arg1, 0x12, 0, 0);
        arg1->panel.field_0.w = 0;
    }
}

void Gp_MapTaskState2(Task* arg0)
{
    UiObject* obj;
    UiObject* child;
    u8*       flags;
    u8        room;

    obj   = arg0->spawnArg2.pointer;
    flags = Gp_MapFlagIds[gGameSession->at4.loc.stage - 1];
    Gp_DrawMapCursor(arg0);
    func_800D0C34(arg0);
    func_800D0614(arg0);
    Gp_DrawMapMarks(arg0);
    func_800D15D0(arg0);
    if (gDisplayState.keepGraphics != 0) {
        Display_SetDrawMode(1);
    } else {
        Display_SetDrawMode(0);
    }
    if (obj->panel.field_0.w == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskCancel | 0x100) != 0) {
            obj->field_2C = 0x101;
            func_800D1F90(arg0);
            obj->field_2E = 6;
            if ((GAME_LOCATION_WORD(gGameSession->at4.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 40, 0, 0)) {
                Gp_LoadViewAndCd(1);
            }
            arg0->state = 3;
            return;
        }
        if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
            func_800D1F90(arg0);
            obj->field_2E = -1;
            if ((GAME_LOCATION_WORD(gGameSession->at4.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 40, 0, 0)) {
                Gp_LoadViewAndCd(1);
            }
            arg0->state = 3;
            return;
        }
        if (Pad_CheckButtons(0, 1, 0x2000) != 0) {
            if (flags[Gp_MapRoomId] == 0xFF) {
                return;
            }
            for (room = Gp_MapRoomId + 1; room <= D_8010F130[gGameSession->at4.loc.stage - 1]; room++) {
                if (func_800D1434(room, flags[room]) == 1) {
                    if ((s8)Gp_MapRoomId != room) {
                        Gp_MapRoomId = room;
                        Display_SetDrawMode(3);
                        SndEvt_EnqueueType6(0x15, 0, 0);
                        Gp_EnqueueMapRoomCd();
                        arg0->state = 1;
                    }
                    return;
                }
            }
        }
        if (Pad_CheckButtons(0, 1, 0x8000) != 0) {
            if (flags[Gp_MapRoomId] == 0xFF) {
                return;
            }
            for (room = Gp_MapRoomId - 1; room != 0; room--) {
                if (func_800D1434(room, flags[room]) == 1) {
                    if ((s8)Gp_MapRoomId != room) {
                        Gp_MapRoomId = room;
                        Display_SetDrawMode(3);
                        SndEvt_EnqueueType6(0x15, 0, 0);
                        Gp_EnqueueMapRoomCd();
                        arg0->state = 1;
                    }
                    return;
                }
            }
        }
        if (Pad_CheckButtons(0, 1, 0x10) != 0) {
            D_8010F13D = func_800E3FCC(0xA2);
            SndEvt_EnqueueType6(3, 0, 0);
            Ui_SpawnFromDesc(&D_8010F15C, 0, 1, 1, obj);
            obj->panel.field_0.w = 0;
        }
    }
    if (arg0->firstChild != NULL) {
        child = arg0->firstChild->spawnArg2.pointer;
        if (child->field_2E == 6) {
            obj->panel.field_0.w = 1;
            Ui_TeardownTree(child, child->owner);
        }
        if (child->field_2E == -1) {
            func_800D1F90(arg0);
            obj->field_2E = -1;
            if ((GAME_LOCATION_WORD(gGameSession->at4.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 40, 0, 0)) {
                Gp_LoadViewAndCd(1);
            }
            arg0->state = 3;
        }
    }
}

static void Gp_DrawMapCursor(Task* arg0)
{
    UiObject*       obj;
    GameActor*      actor;
    GpMapRec*       rec;
    PlayerStatus*   cfg;
    GpMapCursorPos* pos;
    s32             off;
    s32             base;
    SPRT_16*        p;
    DR_TPAGE*       dr;
    s32             u0;
    s32             ang;

    obj   = arg0->spawnArg2.pointer;
    cfg   = &Player_Status;
    actor = gameGetPtrSlot(3)->work;
    rec   = Gp_MapRecTables[gGameSession->at4.loc.stage - 1];
    rec   = rec + gGameSession->at4.loc.area;
    if (rec->field_C != (s8)Gp_MapRoomId) {
        return;
    }

    pos           = SCRATCH_PUSH(GpMapCursorPos);
    pos->field_14 = 0;
    pos->field_12 = 0;
    pos->field_10 = 0;
    off           = (rec->field_0 - cfg->coordMtx->t[0]) / rec->field_8;
    base          = rec->field_4;
    pos->x        = base - off;
    off           = (rec->field_2 - cfg->coordMtx->t[2]) / rec->field_A;
    base          = rec->field_6;
    pos->y        = base + off;

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    ang            = (rsin(gDisplayState.loopCount << 6) + 0x1000) >> 5;
    if (ang == 0x100) {
        ang = 0xFF;
    }
    PRIM_COLOR_WORD(p, 0) = ((ang & 0xFF) << 0x10) | ((ang & 0xFF) << 8) | (ang & 0xFF);
    setlen(p, 3);
    setcode(p, 0x7E);
    p->clut = GetClut(0, 0x101);

    ang = (u16)actor->field_52;
    if (((ang - 0xF00) & 0xFFFF) < 0x100U) {
        u0 = 0x40;
    } else if (ang < 0x100U) {
        u0 = 0x40;
    } else if (((ang - 0x100) & 0xFFFF) < 0x200U) {
        u0 = 0x50;
    } else if (((ang - 0x300) & 0xFFFF) < 0x200U) {
        u0 = 0x60;
    } else if (((ang - 0x500) & 0xFFFF) < 0x200U) {
        u0 = 0x70;
    } else if (((ang - 0x700) & 0xFFFF) < 0x200U) {
        u0 = 0x80;
    } else if (((ang - 0x900) & 0xFFFF) < 0x200U) {
        u0 = 0x90;
    } else if (((ang - 0xB00) & 0xFFFF) < 0x200U) {
        u0 = 0xA0;
    } else if (((ang - 0xD00) & 0xFFFF) < 0x200U) {
        u0 = 0xB0;
    } else {
        goto noDir;
    }
    p->u0 = u0;
    p->v0 = 0x10;
noDir:

    p->x0 = pos->x - 8;
    p->y0 = pos->y - 8;
    addPrim(&gGpuCurrentOt[obj->panel.field_14.s - 0x1C], p);
    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setDrawTPage(dr, 0, 0, 0xE);
    addPrim(&gGpuCurrentOt[obj->panel.field_14.s - 0x1C], dr);
    SCRATCH_POP(GpMapCursorPos);
}

static void func_800D0614(Task* arg0)
{
    UiObject*       obj;
    GpMapCursorPos* pos;
    POLY_FT4*       p;
    SPRT*           sprt;
    DR_TPAGE*       dr;

    obj                          = arg0->spawnArg2.pointer;
    p                            = gGpuPrimCursor;
    pos                          = (GpMapCursorPos*)(SCRATCH_HEAD(u8) - 0x1C);
    SCRATCH_HEAD(GpMapCursorPos) = pos;
    gGpuPrimCursor               = p + 1;
    pos->field_14                = 0;
    pos->field_12                = 0;
    pos->field_10                = 0;
    pos->y                       = 0;
    pos->x                       = 0;
    setPolyFT4(p);
    setRGB0(p, 0x80, 0x80, 0x80);
    p->clut = 0x4000;
    setSemiTrans(p, 1);
    p->tpage = GetTPage(1, 0, 0x380, 0x20);
    p->u0 = p->u2 = 1;
    p->v0 = p->v1 = 0x20;
    p->u3 = p->u1 = 0xFF;
    p->v3 = p->v2 = 0xF0;
    p->x0 = p->x2 = pos->x - 0x7F;
    p->y0 = p->y1 = pos->y - 0x68;
    p->x1 = p->x3 = pos->x + 0x7F;
    p->y2 = p->y3 = pos->y + 0x68;
    addPrim(&gGpuCurrentOt[obj->panel.field_14.s + 2], p);
    SCRATCH_POP_BYTES(0x1C);

    sprt           = gGpuPrimCursor;
    gGpuPrimCursor = sprt + 1;
    setlen(sprt, 4);
    setcode(sprt, 0x67);
    sprt->clut = GetClut(0x70, 0x101);
    sprt->u0   = 0xC0;
    sprt->w    = 0x20;
    sprt->h    = 0x18;
    sprt->x0   = 0x7E;
    sprt->v0   = 0;
    sprt->y0   = -0x64;
    addPrim(&gGpuCurrentOt[obj->panel.field_14.s - 0x19], sprt);
    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setDrawTPage(dr, 0, 0, 0xE);
    addPrim(&gGpuCurrentOt[obj->panel.field_14.s - 0x19], dr);
}

static void Gp_DrawMapMarks(Task* arg0)
{
    Task*        keep;
    GameSession* session;
    GpFlagBank** banks;
    GpFlagBank*  bank;
    s32          flags[2];
    u8*          flagTbl;
    GpMapMark*   recs;
    GpMapMark**  markTable;
    UiObject*    obj;
    s32          color;
    s32          i;
    s32          which;
    s32          bit;
    s32          idx;
    s32          one;
    u8           stage;
    s32          stageM1;

    keep      = arg0;
    color     = 0x5D7;
    session   = gGameSession;
    banks     = Gp_FlagBanks;
    markTable = (keep, Gp_MapMarkTables);
    stage     = session->at4.loc.stage;
    obj       = arg0->spawnArg2.pointer;
    stageM1   = stage - 1;
    bank      = banks[stage];
    recs      = markTable[stageM1];
    flagTbl   = Gp_MapFlagIds[stageM1];
    if (stage == 1) {
        color = 0x83B;
    }
    flags[0] = bank->visitedAreas[0];
    flags[1] = bank->visitedAreas[1];
    if (session->at4.loc.stage == 3) {
        bank      = banks[2];
        flags[0] |= bank->visitedAreas[0];
        flags[1] |= bank->visitedAreas[1];
    }
    i = 0;
    if (Gp_MapMarkCounts[session->at4.loc.stage - 1] != 0) {
        one = 1;
        do {
            if (recs[(u8)i].field_4 == (s8)Gp_MapRoomId) {
                if (recs[(u8)i].field_0 == NULL) {
                    Gp_DrawMapIcons(arg0, (u8)i, 0);
                } else {
                    which = 0;
                    if ((u8)i >= 0x21U) {
                        which = 1;
                        bit   = one << ((u8)i - 0x21);
                    } else {
                        bit = one << ((u8)i - 1);
                    }
                    if (recs[(u8)i].field_5 != 0xFF) {
                        if (recs[(u8)i].field_5 >= 0x21U) {
                            bit |= one << (recs[(u8)i].field_5 - 0x21);
                        } else {
                            bit |= one << (recs[(u8)i].field_5 - 1);
                        }
                    }
                    idx = i;
                    if (Gp_MapRoomOff == 3) {
                        if ((u8)i == 0xE) {
                            idx = 0x22;
                        }
                        if ((u8)i == 0x1B) {
                            idx = 0x23;
                        }
                    }
                    if (GameFlag_GetNibble(flagTbl[Gp_MapRoomId]) == 0) {
                        if ((bit & flags[which]) == 0) {
                            if (Gp_DrawMapIcons(arg0, (u8)i, 1) != 0) {
                                func_800D4270(obj, recs[(u8)idx].field_0, 1, (u16)color);
                            } else {
                                func_800D4270(obj, recs[(u8)idx].field_0, 0, (u16)color);
                            }
                        } else if ((bit & Gp_AreaIdBits[which]) != 0) {
                            func_800D4270(obj, recs[(u8)idx].field_0, 3, (u16)color);
                            Gp_DrawMapIcons(arg0, (u8)i, 0);
                        } else {
                            Gp_DrawMapIcons(arg0, (u8)i, 0);
                        }
                    } else if ((bit & flags[which]) == 0) {
                        func_800D4270(obj, recs[(u8)idx].field_0, 1, (u16)color);
                        Gp_DrawMapIcons(arg0, (u8)i, 1);
                    } else if ((bit & Gp_AreaIdBits[which]) != 0) {
                        func_800D4270(obj, recs[(u8)idx].field_0, 3, (u16)color);
                        Gp_DrawMapIcons(arg0, (u8)i, 0);
                    } else {
                        Gp_DrawMapIcons(arg0, (u8)i, 0);
                    }
                }
            }
            i++;
        } while ((u8)i < Gp_MapMarkCounts[gGameSession->at4.loc.stage - 1]);
    }
}

static void func_800D0C34(Task* arg0)
{
    UiObject*       obj;
    GpMapFlagIcon*  icons;
    GpFlagBank*     bank;
    GpMapCursorPos* pos;
    SPRT_16*        p;
    DR_TPAGE*       dr;
    s32             flags[2];
    u8              i;
    s16             which;
    s32             bit;
    u16             state;
    u8              stage;
    u8              flag;

    i        = 0;
    stage    = gGameSession->at4.loc.stage;
    obj      = arg0->spawnArg2.pointer;
    icons    = D_8010F0E0[stage - 1];
    bank     = Gp_FlagBanks[stage];
    flags[0] = bank->visitedAreas[0];
    flags[1] = bank->visitedAreas[1];
    for (;;) {
        if (icons[i].roomId == 0) {
            return;
        }
        flag = icons[i].flagId;
        if (flag == 0xFF) {
            i++;
            continue;
        }
        if (flag != 0) {
            if (flag >= 0x21) {
                bit   = 1 << (icons[i].flagId - 0x21);
                which = 1;
            } else {
                bit   = 1 << (icons[i].flagId - 1);
                which = 0;
            }
            if (!(bit & flags[which])) {
                i++;
                continue;
            }
        }
        state = Gp_LookupStageFlag(i);
        if (icons[i].roomId != (s8)Gp_MapRoomId) {
            i++;
            continue;
        }
        if (state == 2 || state == 0x802) {
            pos            = SCRATCH_PUSH(GpMapCursorPos);
            pos->field_14  = 0;
            pos->field_12  = 0;
            pos->field_10  = 0;
            pos->x         = icons[i].x;
            p              = gGpuPrimCursor;
            gGpuPrimCursor = p + 1;
            pos->y         = icons[i].y;
            setlen(p, 3);
            setcode(p, 0x7F);
            if (state == 2) {
                p->clut = GetClut(0x30, 0x101);
                p->u0   = 0x60;
                p->v0   = 0;
            } else if (state == 0x802) {
                p->clut = GetClut(0x60, 0x101);
                p->u0   = 0x90;
                p->v0   = 0;
            }
            p->x0 = pos->x - 8;
            p->y0 = pos->y - 8;
            addPrim(&gGpuCurrentOt[obj->panel.field_14.s - 0x1B], p);
            dr             = gGpuPrimCursor;
            gGpuPrimCursor = dr + 1;
            setDrawTPage(dr, 0, 0, 0xE);
            addPrim(&gGpuCurrentOt[obj->panel.field_14.s - 0x1B], dr);
            SCRATCH_POP(GpMapCursorPos);
        }
        i++;
    }
}

static s32 Gp_DrawMapIcons(Task* arg0, u8 arg1, u8 arg2)
{
    UiObject*  obj;
    GpMapIcon* icons;
    u8         i;
    s32        ret;
    u8         otOff;
    s32        lum;

    otOff = 0;
    i     = 0;
    ret   = 0;
    obj   = arg0->spawnArg2.pointer;
    icons = D_8010F0CC[gGameSession->at4.loc.stage - 1];
    lum   = (rsin(gDisplayState.loopCount << 6) + 0x1000) >> 5;

    for (;;) {
        GpMapIconPos* pos;
        SPRT_16*      p;
        DR_TPAGE*     dr;
        u16           clut;

        if (icons[i].field_0 == 0) {
            break;
        }
        if (icons[i].field_2 == 2) {
            if (icons[i].field_3 != func_800E3FCC(0xA2)) {
                i++;
                continue;
            }
        } else if (icons[i].field_3 != 0) {
            if (GameFlag_GetNibble(icons[i].field_3) == 0) {
                i++;
                continue;
            }
        }
        if ((arg2 != 0) && (icons[i].field_2 < 2)) {
            i++;
            continue;
        }
        if ((icons[i].field_0 == (s8)Gp_MapRoomId) && (icons[i].field_1 == arg1)) {
            pos            = SCRATCH_PUSH(GpMapIconPos);
            pos->field_8   = 0;
            pos->field_6   = 0;
            pos->field_4   = 0;
            pos->x         = icons[i].x;
            p              = gGpuPrimCursor;
            gGpuPrimCursor = p + 1;
            pos->y         = icons[i].y;
            if (icons[i].field_2 == 2) {
                if (lum == 0x100) {
                    lum = 0xFF;
                }
                PRIM_COLOR_WORD(p, 0) = ((lum & 0xFF) << 0x10) | ((lum & 0xFF) << 8) | (lum & 0xFF);
            }
            setlen(p, 3);
            setcode(p, 0x7C);
            if (icons[i].field_2 != 2) {
                setcode(p, 0x7D);
            }
            setSemiTrans(p, 1);
            switch (icons[i].field_2) {
                case 0:
                    clut    = GetClut(0x20, 0x101);
                    otOff   = 0x1B;
                    p->clut = clut;
                    p->u0   = 0x50;
                    p->v0   = 0;
                    break;
                case 1:
                    clut    = GetClut(0x10, 0x101);
                    otOff   = 0x1C;
                    p->clut = clut;
                    p->u0   = 0x40;
                    p->v0   = 0;
                    break;
                case 2:
                    clut    = GetClut(0x40, 0x101);
                    otOff   = 0x1C;
                    ret     = 1;
                    p->clut = clut;
                    p->u0   = 0x70;
                    p->v0   = 0;
                    break;
            }
            p->x0 = pos->x - 8;
            p->y0 = pos->y - 8;
            addPrim(&gGpuCurrentOt[obj->panel.field_14.s - otOff], p);
            dr             = gGpuPrimCursor;
            gGpuPrimCursor = dr + 1;
            setDrawTPage(dr, 0, 0, 0xE);
            addPrim(&gGpuCurrentOt[obj->panel.field_14.s - otOff], dr);
            SCRATCH_POP(GpMapIconPos);
        }
        i++;
    }
    return ret;
}

static void Gp_EnqueueMapRoomCd(void)
{
    u8  param1[8];
    u8  param2[8];
    s32 room;
    u8  stage;

    Gp_MapRoomOff             = 0;
    gGameSession->loadedSndId = 0;
    if ((gGameSession->at4.loc.stage == 4) && ((s8)Gp_MapRoomId == 6) && (GameFlag_GetNibble(0xB7) == 0)) {
        Gp_MapRoomOff = 1;
    }
    if (gGameSession->at4.loc.stage == 5) {
        room = (s8)Gp_MapRoomId;
        if ((room == 1) && (GameFlag_GetNibble(0xD9) == room)) {
            Gp_MapRoomOff = 3;
        }
    }
    param1[2] = 3;
    param1[3] = 0;
    param1[0] = Gp_MapRoomId + Gp_MapRoomOff;
    stage     = gGameSession->at4.loc.stage;
    param2[1] = 0;
    param2[3] = 0;
    param2[2] = 0;
    param2[0] = stage;
    CdCmd_Enqueue(0x21, param1, param2);
    D_800626E8 = 1;
}

static s8 func_800D1434(u32 roomId, u8 flagId)
{
    GpFlagBank* bank;
    GpMapRec*   recs;
    s32         flags[2];
    s32         i;
    s32         which;
    s32         bit;
    s32         one;
    s32         skip;

    bank = Gp_FlagBanks[gGameSession->at4.loc.stage];
    if (gGameSession->at4.loc.stage != 5) {
        if (flagId != 0xFF) {
            if (flagId == 0x80) {
                return 0;
            }
            which = flagId != 0;
            if (which && (GameFlag_GetNibble(flagId) != 0)) {
                return 1;
            }
            recs     = Gp_MapRecTables[gGameSession->at4.loc.stage - 1];
            flags[0] = bank->visitedAreas[0];
            flags[1] = bank->visitedAreas[1];
            i        = 0;
            if (gGameSession->at4.loc.stage == 3) {
                bank      = Gp_FlagBanks[2];
                flags[0] |= bank->visitedAreas[0];
                flags[1] |= bank->visitedAreas[1];
            }
            if (recs->field_C != 0xFFFF) {
                skip = 0xF000;
                one  = 1;
                do {
                    recs++;
                    i++;
                    if (recs->field_C != skip) {
                        which = 0;
                        if ((u8)i >= 0x21U) {
                            which = 1;
                            bit   = one << ((u8)i - 0x21);
                        } else {
                            bit = one << ((u8)i - 1);
                        }
                        if (bit & flags[which]) {
                            if (recs->field_C == (u8)roomId) {
                                return 1;
                            }
                        }
                    }
                } while (recs->field_C != 0xFFFF);
            }
        }
    }
    return 0;
}

static void func_800D15D0(Task* arg0)
{
    UiObject* obj;
    u8*       flagIds;
    SPRT*     p;
    DR_TPAGE* dr;
    DR_TPAGE* rightDr;
    s32       i;
    s32       lum;
    s8        ret;
    u8        stage;

    stage   = gGameSession->at4.loc.stage;
    obj     = arg0->spawnArg2.pointer;
    flagIds = Gp_MapFlagIds[stage - 1];
    if (stage == 5) {
        return;
    }
    if (flagIds[Gp_MapRoomId] == 0xFF) {
        return;
    }

    i = Gp_MapRoomId - 1;
    while ((u8)i != 0) {
        if (flagIds[(u8)i] == 0) {
            break;
        }
        if (flagIds[(u8)i] == 0xFF) {
            break;
        }
        ret = func_800D1434((u8)i, flagIds[(u8)i]);
        if (ret == 1) {
            p              = gGpuPrimCursor;
            gGpuPrimCursor = p + 1;
            lum            = (rsin(gDisplayState.loopCount << 5) + 0x1000) >> 5;
            if (lum == 0x100) {
                lum = 0xFF;
            }
            PRIM_COLOR_WORD(p, 0) = ((lum & 0xFF) << 0x10) | ((lum & 0xFF) << 8) | (lum & 0xFF);
            setlen(p, 4);
            setcode(p, 0x66);
            p->clut = GetClut(0x50, 0x101);
            p->u0   = 0x88;
            p->w    = 8;
            p->h    = 0x10;
            p->x0   = -0x89;
            p->v0   = 0;
            p->y0   = -7;
            addPrim(&gGpuCurrentOt[obj->panel.field_14.s - 0x1C], p);
            dr             = gGpuPrimCursor;
            gGpuPrimCursor = dr + 1;
            setDrawTPage(dr, 0, 0, 0xE);
            addPrim(&gGpuCurrentOt[obj->panel.field_14.s - 0x1C], dr);
            break;
        }
        i--;
    }

    i = Gp_MapRoomId + 1;
    while ((u8)i <= D_8010F130[gGameSession->at4.loc.stage - 1]) {
        if (flagIds[(u8)i] == 0) {
            return;
        }
        if (flagIds[(u8)i] == 0xFF) {
            return;
        }
        ret = func_800D1434((u8)i, flagIds[(u8)i]);
        if (ret == 1) {
            p              = gGpuPrimCursor;
            gGpuPrimCursor = p + 1;
            lum            = (rsin(gDisplayState.loopCount << 5) + 0x1000) >> 5;
            if (lum == 0x100) {
                lum = 0xFF;
            }
            PRIM_COLOR_WORD(p, 0) = ((lum & 0xFF) << 0x10) | ((lum & 0xFF) << 8) | (lum & 0xFF);
            setlen(p, 4);
            setcode(p, 0x66);
            p->clut = GetClut(0x50, 0x101);
            p->u0   = 0x80;
            p->w    = 8;
            p->h    = 0x10;
            p->x0   = 0x82;
            p->v0   = 0;
            p->y0   = -7;
            addPrim(&gGpuCurrentOt[obj->panel.field_14.s - 0x1C], p);
            rightDr        = gGpuPrimCursor;
            gGpuPrimCursor = rightDr + 1;
            setDrawTPage(rightDr, 0, 0, 0xE);
            addPrim(&gGpuCurrentOt[obj->panel.field_14.s - 0x1C], rightDr);
            return;
        }
        i++;
    }
}

void Gp_HelpPanelTask(Task* arg0)
{
    UiObject* obj;
    s32       status;

    obj           = arg0->spawnArg2.pointer;
    obj->field_2E = 0;
    Ui_DrawText(&(obj)->panel, Gp_StrHelp);
    switch (arg0->state) {
        case 0:
            CdCmd_EnqueueLoadFile(8, D_8010F13D, 0);
            arg0->state = arg0->state + 1;
            break;
        case 1:
            if (CdCmd_IsIdle() & 0xFFFF) {
                Ui_SpawnFromDesc(&D_8010F178, 0, 0, 1, obj);
                arg0->state = arg0->state + 1;
            }
            break;
        case 2:
            Text_DrawMultiLine(obj, obj->panel.field_1C.s + 2, (s16)obj->panel.field_18.u + 0x14, Fs_GetChunkPayload(), 0x606060, 1, 0);
            status = obj->panel.field_0.w;
            if (status == 1) {
                if (Pad_CheckButtons(0, 1, Pad_MaskCancel | 0x10) != 0) {
                    obj->field_2C = status;
                    obj->field_2E = 6;
                    SndEvt_EnqueueType6(4, 0, 0);
                } else if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
                    obj->field_2E = -1;
                    SndEvt_EnqueueType6(4, 0, 0);
                }
            }
            break;
    }
}

void Gp_DrawMapName(Task* arg0)
{
    TextDrawReq  req;
    TextDrawReq  req2;
    GameSession* session;
    GpMapName*   names;
    u8*          text;
    UiObject*    obj;
    s32          width;

    session = gGameSession;
    names   = Gp_MapNameTables[session->at4.loc.stage - 1];
    obj     = arg0->spawnArg2.pointer;
    if (names != NULL) {
        text = names[session->at4.loc.area - 1].text;
        if (arg0->state == 0) {
            req.x          = 0;
            req.y          = 0;
            req.otIndex    = obj->panel.field_14.s + 1;
            req.field_8    = 0;
            req.glyphTable = 4;
            req.centerMode = 2;
            req.field_E    = 0;
            Text_MeasureAndCenter(&req, text);
            width = -req.x + 4;
            Ui_UpdateLayoutSize(&(obj)->panel, width, Ui_Scale15(1));
            arg0->state = arg0->state + 1;
        }
        req2.x          = (u16)obj->panel.field_1C.s + (obj->panel.field_20.u + 2);
        req2.y          = (u16)obj->panel.field_18.u + (obj->panel.field_22.u + 0xB);
        req2.otIndex    = obj->panel.field_14.s + 1;
        req2.field_8    = 0x806020;
        req2.glyphTable = 4;
        req2.centerMode = 0;
        req2.field_E    = 1;
        Text_DrawString(&req2, text);
    }
}

void Gp_MapTask(Task* arg0)
{
    TaskFuncTable4 sp;

    sp = Gp_MapTaskStates;
    sp.funcs[arg0->state](arg0);
}

void Gp_MapPanelInit(Task* arg0)
{
    RECT         rect;
    GameSession* session;
    GpMapRec**   table;
    s32          idx;
    u8           f6;
    GpMapRec*    recs;
    u8           val;

    if (gDisplayState.keepGraphics == 0) {
        rect.x = 0x380;
        rect.w = 0x80;
        rect.y = 0;
        rect.h = 0x100;
        Display_SetDrawMode(0);
        StoreImage2(&rect, (u_long*)(Gpu_PrimHeapBase - 0x25800));
    }
    Gp_RebuildAreaIdBits();
    session      = gGameSession;
    table        = Gp_MapRecTables;
    idx          = session->at4.loc.stage - 1;
    f6           = session->at4.loc.area;
    recs         = table[idx];
    recs         = recs + f6;
    val          = recs->field_C;
    Gp_MapRoomId = val;
    Gp_EnqueueMapRoomCd();
    arg0->state = arg0->state + 1;
}

void Gp_MapFirstDrawTask(Task* arg0)
{
    UiObject* obj;

    obj = arg0->spawnArg2.pointer;
    if (CdCmd_IsIdle() & 0xFFFF) {
        obj->panel.field_16 = 1;
        Gp_DrawMapCursor(arg0);
        func_800D0C34(arg0);
        func_800D0614(arg0);
        Gp_DrawMapMarks(arg0);
        func_800D15D0(arg0);
        arg0->state = arg0->state + 1;
    } else {
        obj->panel.field_16 = (u16)obj->panel.field_16 + 1;
    }
}

void Gp_MapDrawTask(Task* arg0)
{
    RECT rect;

    if (arg0->spawnArg1.value != 0) {
        return;
    }

    arg0->killCountdown--;
    if (arg0->killCountdown == 0) {
        if (gDisplayState.keepGraphics == 0) {
            rect.x = 0x380;
            rect.w = 0x80;
            rect.y = 0;
            rect.h = 0x100;
            Gp_LoadViewImages();
            LoadImage2(&rect, (u_long*)(Gpu_PrimHeapBase - 0x25800));
        }
        arg0->spawnArg1.value++;
    } else if (arg0->killCountdown >= 2) {
        Gp_DrawMapCursor(arg0);
        func_800D0C34(arg0);
        func_800D0614(arg0);
        Gp_DrawMapMarks(arg0);
    }
}

static void func_800D1F90(Task* arg0)
{
    UiObject* obj;

    obj = arg0->spawnArg2.pointer;
    GameMain_SetFrameTiming(DISPLAY_TIMING_TWO_VBLANKS);
    arg0->killCountdown   = 4;
    obj->panel.field_16   = 0;
    arg0->spawnArg1.value = 0;
}

static u8 Gp_GetMapRoomId(void)
{
    GameSession* session;
    GpMapRec**   table;
    s32          idx;
    u8           f6;
    GpMapRec*    recs;

    session = gGameSession;
    table   = Gp_MapRecTables;
    idx     = session->at4.loc.stage - 1;
    f6      = session->at4.loc.area;
    recs    = table[idx];
    recs    = recs + f6;

    Gp_MapRoomId = recs->field_C;
    return Gp_MapRoomId;
}

static void func_800D2020(u8 arg0)
{
    RECT rect;

    if (gDisplayState.keepGraphics != 0) {
        return;
    }

    rect.x = 0x380;
    rect.w = 0x80;
    rect.h = 0x100;
    rect.y = 0;
    if (arg0 == 0) {
        Display_SetDrawMode(0);
        StoreImage2(&rect, (u_long*)(Gpu_PrimHeapBase - 0x25800));
    } else {
        Gp_LoadViewImages();
        LoadImage2(&rect, (u_long*)(Gpu_PrimHeapBase - 0x25800));
    }
}

void Gp_PeMenuListTask(Task* arg0)
{
    UiObject* obj;
    UiList*   menu;
    Task*     owner;
    Task*     child;
    Task*     next;
    Task*     head;
    UiObject* childObj;
    s32       flag;

    obj           = arg0->spawnArg2.pointer;
    owner         = obj->owner;
    obj->field_2E = 0;
    menu          = &D_8010F5D0;
    if (owner->state == 0) {
        Ui_LayoutListPanel(menu, &(obj)->panel);
        owner->state = owner->state + 1;
    }
    Ui_UpdateListNoAnim(menu, obj);
    if (obj->panel.field_0.w == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(4, 0, 0);
            obj->field_2E = 6;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
            obj->field_2E = -1;
        }
    }
    head = owner->firstChild;
    if (head != NULL) {
        child = head;
        do {
            childObj = child->spawnArg2.pointer;
            flag     = childObj->field_2E;
            next     = child->nextSibling;
            switch (flag) {
                case 6:
                    obj->panel.field_0.w = 1;
                    Ui_TeardownTree(childObj, childObj->owner);
                    break;
                case 9:
                    obj->field_2E = 6;
                    break;
                case -1:
                    obj->field_2E = flag;
                    break;
            }
            child = next;
        } while (child != owner->firstChild);
    }
}

void Gp_DrawReviveCmd(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;
    s32         flags;
    s32         item;

    flags = arg1->owner->spawnArg1.value;
    if (flags & 3) {
        req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
        req.y          = arg1->panel.field_22.u + (u16)arg0->field_1A;
        req.otIndex    = arg1->panel.field_14.s + 1;
        req.field_8    = arg0->field_1C;
        req.glyphTable = 0;
        req.centerMode = 0;
        req.field_E    = 1;
        Text_DrawString(&req, Gp_StrStrengthen);
    } else {
        req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
        req.y          = arg1->panel.field_22.u + (u16)arg0->field_1A;
        req.otIndex    = arg1->panel.field_14.s + 1;
        req.field_8    = arg0->field_1C;
        req.glyphTable = 0;
        req.centerMode = 0;
        req.field_E    = 1;
        Text_DrawString(&req, Gp_StrRevive);
    }
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            item = -1;
            SndEvt_EnqueueType6(3, 0, 0);
            if ((flags & 3) == 3) {
                item = 0xD;
            }
            if (item >= 0) {
                Gp_SpawnItemPrompt(arg1, item, 0, 0);
                arg1->panel.field_0.w = 0;
            } else {
                Ui_SpawnFromDesc(&D_8010F7A4, flags, 1, 0xA, arg1);
                arg1->panel.field_0.w = 0;
            }
        }
    }
}

void Gp_PeCommandMenuTask(Task* arg0)
{
    UiObject*       obj;
    UiList*         menu;
    Task*           owner;
    Task*           child;
    Task*           next;
    Task*           head;
    UiObject*       childObj;
    UiListItemFunc* table;
    s32             flag;
    s32             two;

    menu = &D_8010F5FC;
    obj  = arg0->spawnArg2.pointer;
    if (arg0->state == 0) {
        table           = menu->funcs;
        table[0]        = Gp_DrawReviveCmd;
        table[1]        = Gp_DrawPeSlotCmd;
        two             = 2;
        menu->field_5.u = two;
        menu->field_4   = two;
        if ((arg0->spawnArg1.value & 3) != 3) {
            Gp_SetPreviewItem(arg0->spawnArg1.value + 1, 0);
        }
    }
    owner         = obj->owner;
    obj->field_2E = 0;
    if (owner->state == 0) {
        Ui_LayoutListPanel(menu, &(obj)->panel);
        owner->state = owner->state + 1;
    }
    Ui_UpdateListNoAnim(menu, obj);
    if (obj->panel.field_0.w == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(4, 0, 0);
            obj->field_2E = 6;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
            obj->field_2E = -1;
        }
    }
    head = owner->firstChild;
    if (head != NULL) {
        child = head;
        do {
            childObj = child->spawnArg2.pointer;
            flag     = childObj->field_2E;
            next     = child->nextSibling;
            switch (flag) {
                case 6:
                    obj->panel.field_0.w = 1;
                    Ui_TeardownTree(childObj, childObj->owner);
                    break;
                case 9:
                    obj->field_2E = 6;
                    break;
                case -1:
                    obj->field_2E = flag;
                    break;
            }
            child = next;
        } while (child != owner->firstChild);
    }
}

void Gp_DiscardWarnTask(Task* arg0)
{
    McItemRec* rec;
    s32        id;
    Task*      child;
    UiObject*  childObj;
    UiObject*  parentObj;
    UiObject*  obj;
    s32        mode;
    u8*        text;
    UiObject*  spawned;

    rec = Gp_SelItemRec;
    id  = rec->itemId;

    obj           = arg0->spawnArg2.pointer;
    obj->field_2E = 0;
    mode          = 0x10;
    if (Gp_ItemDescs[id].field_3 & 1) {
        mode = 1;
    } else if (((u32)(id - 0xA0) < 0x20U) && (Gp_CountEquippedRelated(&Mc_SaveData[0].state.carriedItems, id) > 0)) {
        mode = 3;
    } else if (Gp_IsEquippedItem(id) != 0) {
        mode = 2;
    }
    if (mode != 0x10) {
        arg0->spawnArg1.value = mode;
        Gp_NoticePanelTask(arg0);
        return;
    }
    text = Gp_PromptTexts[0];
    if (arg0->state == 0) {
        Ui_SizeFromTextWide(&(obj)->panel, text);
        spawned = func_800CD89C(obj);
        if (spawned != NULL) {
            spawned->panel.bounds.unsignedRect.x = (obj->panel.bounds.unsignedRect.x + obj->panel.bounds.unsignedRect.w) - 0x18;
        }
        arg0->state += 1;
    }
    Ui_DrawTextColored(&(obj)->panel, Gp_StrAttention2);
    Text_DrawMultiLine(obj, obj->panel.field_1C.s + 2, (s16)obj->panel.field_18.u + 0xF, text, 0x606060, 1, 0);

    child = arg0->firstChild;
    if (child != NULL) {
        childObj = child->spawnArg2.pointer;
        if (childObj->field_2E == 6) {
            parentObj = arg0->parent->spawnArg2.pointer;
            if (childObj->field_2C == 0x33) {
                if ((u32)(id - 0x80) < 0x20U) {
                    McItemSlot*   slot;
                    PlayerStatus* cfg;

                    slot = Gp_GetItemSlot(id);
                    cfg  = &Player_Status;
                    Gp_ClearEquipSlot(id);
                    slot->field_4 = 0;
                    if (cfg->weapon == (id - 0x7F)) {
                        cfg->weapon = 0;
                    }
                } else if ((u32)(id - 0xA0) < 0x20U) {
                    s32         i;
                    McItemSlot* slot;

                    i = 0x80;
                    do {
                        slot = Gp_GetItemSlot(i);
                        if (slot->ammoId == id) {
                            slot->ammoId  = 0;
                            slot->ammoQty = 0;
                        }
                        if (slot->attachId == id) {
                            slot->attachId  = 0;
                            slot->attachQty = 0;
                        }
                        i += 1;
                    } while (i < 0xA0);
                } else if ((u32)(id - 0x60) < 0x20U) {
                    PlayerStatus* cfg;

                    Mc_SaveData[0].state.itemLevelBonus[id - 0x60] = 0;
                    cfg                                            = &Player_Status;
                    if (cfg->armor == (id - 0x5F)) {
                        cfg->armor = 0;
                    }
                }
                Gp_RemoveItem(&Mc_SaveData[0].state.carriedItems, rec, -1);
            }
            parentObj->field_2E = 6;
        }
    }
}

void Gp_DrawPeSlotRow(UiList* arg0, UiObject* arg1)
{
    s32       count;
    s32       item;
    s32       idx;
    s32       slot;
    s32       off;
    s32       base;
    s32       status;
    s32       one;
    UiObject* obj;

    idx   = arg1->owner->spawnArg1.value;
    slot  = arg0->field_8;
    count = Mc_SaveData[0].state.attachLevels[slot + idx * 3];
    off   = idx * 16;
    base  = slot * 4 + 0x300;
    item  = off + base + count;
    if (count == 0) {
        arg0->field_1C = Ui_LookupTable(arg1, 2);
    }
    Gp_DrawItemLabel(arg1, arg0->field_18, arg0->field_1A, item, arg0->field_1C, 0);
    if (count != 0) {
        func_800C2538(arg1, arg0->field_18, arg0->field_1A, count, arg0->field_1C);
    }
    status = arg1->panel.field_0.w;
    one    = 1;
    if (((status >> 16) == one) || (status == one)) {
        if (arg0->field_10 == arg0->field_8) {
            Ui_SetHolderParamAlt(item, 0, 0);
        }
    }
    if (arg0->field_C == 1) {
        Gp_SetPreviewItem(item, 0);
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            one = 1;
            obj = Ui_SpawnFromDesc(&D_8010F670, item, one, one, arg1);
            SndEvt_EnqueueType6(3, 0, 0);
            if (obj != NULL) {
                Ui_ClampDialogRect(&(obj)->panel, arg0, &(arg1)->panel);
                arg1->panel.field_0.w = 0;
            }
        } else if (Pad_CheckButtons(0, 1, 0x10) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            one = 1;
            Ui_SpawnFromDesc(&D_8010F7F8, item, one, one, arg1);
            arg1->panel.field_0.w = 0;
        }
    }
}

void func_800D29B0(Task* arg0)
{
    UiObject* obj;
    UiList*   menu;
    Task*     child;
    UiObject* childObj;
    u8*       levels;
    s32       flag;
    s32       last;
    s32       textIndex;

    obj          = arg0->spawnArg2.pointer;
    textIndex    = arg0->spawnArg1.value;
    arg0->status = 0;
    menu         = &D_80114DF8[arg0->spawnArg1.value];
    Ui_DrawText(&(obj)->panel, D_8010F644[textIndex]);
    if (arg0->state == 0) {
        menu->funcs     = D_8010F620;
        menu->field_4   = 3;
        menu->field_5.u = 3;
        menu->field_6   = 0;
        menu->field_7   = 0xF;
        menu->field_8   = 0;
        menu->field_9.u = 0;
        Ui_LayoutListPanel(menu, &(obj)->panel);
        levels = &Mc_SaveData[0].state.attachLevels[arg0->spawnArg1.value * 3];
        if (levels[2] != 0 || (levels[0] == 3 && levels[1] == levels[0])) {
            menu->field_4 = menu->field_5.u = 3;
        } else {
            menu->field_4 = menu->field_5.u = 2;
        }
        menu->field_9.u = 0;
        menu->field_10  = 0;
        menu->field_A   = 1;
        if (arg0->spawnArg1.value & 1) {
            obj->panel.bounds.unsignedRect.y = obj->panel.bounds.unsignedRect.h - 0x50;
        }
        arg0->state++;
    }
    levels = &Mc_SaveData[0].state.attachLevels[arg0->spawnArg1.value * 3];
    if (levels[2] != 0 || (levels[0] == 3 && levels[1] == levels[0])) {
        menu->field_4 = menu->field_5.u = 3;
    } else {
        menu->field_4 = menu->field_5.u = 2;
    }
    Ui_UpdateListNoAnim(menu, obj);
    if (obj->panel.field_0.w == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(4, 0, 0);
            obj->field_2E = 6;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
            obj->field_2E = -1;
        } else if (menu->field_22 == 3 && !(arg0->spawnArg1.value & 1)) {
            UiObject* verticalObj;
            UiList*   verticalMenu;

            verticalObj  = arg0->nextSibling->spawnArg2.pointer;
            verticalMenu = &D_80114DF8[arg0->spawnArg1.value] + 1;
            SndEvt_EnqueueType6(2, 0, 0);
            verticalObj->panel.field_0.w = 0x17;
            verticalMenu->field_10       = 0;
            obj->panel.field_0.w         = 0;
        } else if (menu->field_22 == 2 && (arg0->spawnArg1.value & 1)) {
            UiObject* verticalObj;
            UiList*   verticalMenu;

            verticalObj  = arg0->nextSibling->nextSibling->nextSibling->spawnArg2.pointer;
            verticalMenu = &D_80114DF8[arg0->spawnArg1.value] - 1;
            SndEvt_EnqueueType6(2, 0, 0);
            verticalObj->panel.field_0.w = 0x17;
            verticalMenu->field_10       = verticalMenu->field_4 - 1;
            obj->panel.field_0.w         = 0;
        } else if (Pad_CheckButtons(0, 1, 0x5000) == 0) {
            if (!(arg0->spawnArg1.value & 2)) {
                if (Pad_CheckButtons(0, 1, 0x2000) != 0) {
                    UiObject* nextObj;
                    UiList*   nextMenu;

                    nextObj  = arg0->nextSibling->nextSibling->spawnArg2.pointer;
                    nextMenu = &D_80114DF8[arg0->spawnArg1.value] + 2;
                    SndEvt_EnqueueType6(2, 0, 0);
                    nextObj->panel.field_0.w = 0x17;
                    last                     = nextMenu->field_4 - 1;
                    nextMenu->field_10       = menu->field_10;
                    if (last < nextMenu->field_10) {
                        nextMenu->field_10 = last;
                    }
                    obj->panel.field_0.w = 0;
                }
            } else if (Pad_CheckButtons(0, 1, 0x8000) != 0) {
                UiObject* nextObj;
                UiList*   nextMenu;

                nextObj  = arg0->nextSibling->nextSibling->spawnArg2.pointer;
                nextMenu = &D_80114DF8[arg0->spawnArg1.value] - 2;
                SndEvt_EnqueueType6(2, 0, 0);
                nextObj->panel.field_0.w = 0x17;
                last                     = nextMenu->field_4 - 1;
                nextMenu->field_10       = menu->field_10;
                if (last < nextMenu->field_10) {
                    nextMenu->field_10 = last;
                }
                obj->panel.field_0.w = 0;
            }
        }
    }
    child = arg0->firstChild;
    if (child != NULL) {
        childObj = child->spawnArg2.pointer;
        flag     = childObj->field_2E;
        if (flag == 6) {
            Ui_TeardownTree(childObj, childObj->owner);
            obj->panel.field_0.w = 1;
        } else if (flag == -1) {
            obj->field_2E = flag;
        }
    }
    if (obj->panel.field_0.w == 0x17) {
        obj->panel.field_0.w = 1;
    }
}

void Gp_DrawCastCostLines(UiObject* arg0, s32 arg1)
{
    TextDrawReq req;
    s16         y;
    s32         mask;
    s32         lineY;
    s32         color;
    s32         one;
    u8*         text;

    y     = arg0->panel.field_18.u;
    mask  = arg1 & 3;
    lineY = y + 0xF;
    text  = Gp_GetItemText(arg1, 1, 1);
    color = 0x606060;
    one   = 1;
    Text_DrawPrompt(arg0, arg0->panel.field_1C.s + 2, lineY, text, color, one, 0);
    text = Gp_GetItemText(arg1, 2, one);
    Text_DrawPrompt(arg0, arg0->panel.field_1C.s + 2, y + 0x1E, text, color, one, 0);
    y     = arg0->panel.field_18.u;
    lineY = y + 0xF;
    Ui_DrawVBar(&(arg0)->panel, y, (s16)arg0->panel.field_1A.u, 0x2F);
    if (mask) {
        req.x          = arg0->panel.field_20.u + 0x34;
        req.y          = (s16)(arg0->panel.field_22.u - 6) + lineY;
        req.otIndex    = arg0->panel.field_14.s + 1;
        req.field_8    = color;
        req.glyphTable = 0;
        req.centerMode = 0;
        req.field_E    = 1;
        Text_DrawString(&req, Gp_StrCastCost);
        func_800D3660(arg0, arg1, 0, 0x34, y + 0x1A, 2);
    }
}

void Gp_NoticePanelTask(Task* arg0)
{
    UiObject* obj;
    s32       one;
    u8*       text;
    s32       color;

    obj           = arg0->spawnArg2.pointer;
    obj->field_2E = 0;
    Ui_DrawText(&(obj)->panel, Gp_StrNotice3);

    color = 0x606060;
    text  = Gp_NoticeTexts[(u16)arg0->spawnArg1.value];

    if (arg0->state == 0) {
        Ui_SizeFromTextPlain(&(obj)->panel, text);
        arg0->killCountdown = 0xBC;
        arg0->state         = arg0->state + 1;
    }

    one = 1;
    Text_DrawMultiLine(obj, obj->panel.field_1C.s + 2, (s16)obj->panel.field_18.u + 0xF, text, color, one, 0);

    arg0->killCountdown--;
    if (obj->panel.field_0.w == one) {
        if ((arg0->killCountdown <= 0) || (Pad_CheckButtons(0, one, Pad_MaskConfirm | Pad_MaskCancel) != 0)) {
            obj->field_2E       = 6;
            arg0->killCountdown = 0x7FFF;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
            obj->field_2E = -1;
        }
    }

    if ((s16)(arg0->spawnArg1.value >> 16) == 0) {
        if (obj->field_2E == 6) {
            obj->field_2E = 9;
        }
    }
}

void Gp_PeUpgradePanelTask(Task* arg0)
{
    u8            str[0x20];
    TextDrawReq   req;
    TextDrawReq   req2;
    TextDrawReq   req3;
    TextDrawReq   req4;
    UiObject*     obj;
    UiObject*     frame;
    UiObject*     childObj;
    Task*         child;
    Task*         next;
    s32           id;
    s32           row2;
    s32           col2;
    s32           lvl2;
    s32           row3;
    s32           col3;
    s32           lvl3;
    s32           row;
    s32           col;
    s32           lvl;
    s32           y;
    s32           bonusIdx;
    s32           x;
    s32           cost;
    PlayerStatus* cfg;
    s32           price;

    obj           = arg0->spawnArg2.pointer;
    id            = arg0->spawnArg1.value;
    obj->field_2E = 0;

    if (arg0->state == 0) {
        Ui_UpdateLayoutSize(&(obj)->panel, 0, Ui_Scale15(2) + 1);
        frame = func_800CD814(obj);
        if (frame != NULL) {
            frame->panel.field_16               = obj->panel.field_16 - 8;
            frame->panel.bounds.unsignedRect.y += 4;
        }
        Ui_SpawnFromDesc(D_8010F7C0, id, 0, 1, obj);
        arg0->state = arg0->state + 1;
    }

    x = 0x20;
    y = (s16)obj->panel.field_18.u + 0xF;

    req.x          = obj->panel.field_20.u + x;
    req.y          = (s16)(obj->panel.field_22.u - 8) + y;
    req.otIndex    = obj->panel.field_14.s + 1;
    req.field_8    = 0x606060;
    req.glyphTable = 5;
    req.centerMode = 2;
    req.field_E    = 1;
    Text_DrawString(&req, D_8009720C);

    req2.x          = obj->panel.field_20.u + x;
    req2.y          = (s16)(obj->panel.field_22.u - 2) + y;
    req2.otIndex    = obj->panel.field_14.s + 1;
    req2.field_8    = 0x606060;
    req2.glyphTable = 5;
    req2.centerMode = 2;
    req2.field_E    = 1;
    Text_DrawString(&req2, Gp_StrCost);

    row  = ((id + 1) & 0x30) >> 4;
    col  = ((id + 1) & 0xC) >> 2;
    lvl  = (id + 1) & 3;
    cost = Gp_IdParamHi.rows[(row * 3 + col) * 3 + lvl].field[0];
    if (Mc_SaveData[0].state.gameMode > 0) {
        cost = (cost * 4) / 5;
    } else if (Mc_SaveData[0].state.clearCount > 0) {
        cost = (cost * 2) / 5;
    }
    Text_DrawPrompt(obj, x + 0x30, y, Text_ItoaSigned(str, cost & 0xFFFF), 0x606060, 3, 2);

    y += 0xF;

    req3.x          = obj->panel.field_20.u + x;
    req3.y          = (s16)(obj->panel.field_22.u - 8) + y;
    req3.otIndex    = obj->panel.field_14.s + 1;
    req3.field_8    = 0x606060;
    req3.glyphTable = 5;
    req3.centerMode = 2;
    req3.field_E    = 1;
    Text_DrawString(&req3, Gp_StrBonus);

    req4.x          = obj->panel.field_20.u + x;
    req4.y          = (s16)(obj->panel.field_22.u - 2) + y;
    req4.otIndex    = obj->panel.field_14.s + 1;
    req4.field_8    = 0x606060;
    req4.glyphTable = 5;
    req4.centerMode = 2;
    req4.field_E    = 1;
    Text_DrawString(&req4, D_80097220);

    bonusIdx = 1;
    row2     = ((id + 1) & 0x30) >> 4;
    col2     = ((id + 1) & 0xC) >> 2;
    lvl2     = (id + 1) & 3;
    Text_DrawPrompt(obj, x + 0x30, y, Text_ItoaSigned(str, Gp_IdParamHi.rows[(row2 * 3 + col2) * 3 + lvl2].field[bonusIdx]),
                    0x606060, 3, 2);

    if (arg0->firstChild != NULL) {
        child = arg0->firstChild;
        do {
            childObj = child->spawnArg2.pointer;
            next     = child->nextSibling;
            row3     = ((id + 1) & 0x30) >> 4;
            col3     = ((id + 1) & 0xC) >> 2;
            lvl3     = (id + 1) & 3;
            if (childObj->field_2E == 6) {
                if (childObj->field_2C == 0x33) {
                    cfg   = &Player_Status;
                    price = Gp_IdParamHi.rows[(row3 * 3 + col3) * 3 + lvl3].field[0];
                    if (Mc_SaveData[0].state.gameMode > 0) {
                        price = (price * 4) / 5;
                    } else if (Mc_SaveData[0].state.clearCount > 0) {
                        price = (price * 2) / 5;
                    }
                    if (cfg->exp < (price & 0xFFFF)) {
                        Ui_SpawnFromDesc(&D_8010F788, 0xC, 1, 1, obj);
                        Ui_TeardownTree(childObj, childObj->owner);
                    } else {
                        price = Gp_IdParamHi.rows[(row3 * 3 + col3) * 3 + lvl3].field[0];
                        if (Mc_SaveData[0].state.gameMode > 0) {
                            price = (price * 4) / 5;
                        } else if (Mc_SaveData[0].state.clearCount > 0) {
                            price = (price * 2) / 5;
                        }
                        cfg->exp                                                                     -= price & 0xFFFF;
                        Mc_SaveData[0].state.attachLevels[((id & 0xC) >> 2) + ((id & 0x30) >> 4) * 3] = (id & 3) + 1;
                        Gp_RecalcMaxMp();
                        cfg->mp             = cfg->mpMax;
                        Gp_HpMpWork.field_4 = cfg->mp;
                        obj->field_2E       = 9;
                    }
                } else {
                    obj->field_2E = 9;
                }
            } else if (childObj->field_2E == 9) {
                obj->field_2E = 9;
            }
            child = next;
        } while (child != arg0->firstChild);
    }
}

static void func_800D3660(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5)
{
    struct
    {
        u8          buf[0x20];
        TextDrawReq req;
        TextDrawReq req2;
    } loc;
    s32   span;
    s32   width;
    s32   raw;
    s32   val;
    s32   max;
    s32   x;
    SPRT* p;
    s32   rawPrev;
    s32   prev;
    s32   color;
    s32   textY;
    s32   prevId;
    s32   spriteX;
    s32   a;
    s32   b;
    s32   c;
    s32   aPrev;
    s32   bPrev;
    s32   cPrev;
    s32   textY2;
    u8*   text;
    {
        a   = (arg1 & 0x30) >> 4;
        b   = (arg1 & 0xC) >> 2;
        c   = arg1 & 3;
        raw = Gp_IdParamHi.rows[(((a * 3) + b) * 3) + c].field[arg5];
    }
    if (arg5 == 0) {
        if (Mc_SaveData[0].state.gameMode > 0) {
            raw = (raw * 4) / 5;
        } else if (Mc_SaveData[0].state.clearCount > 0) {
            raw = (raw * 2) / 5;
        }
        width = (s16)arg0->panel.field_1E.u;
    } else {
        width = (s16)arg0->panel.field_1E.u;
    }
    x    = arg3;
    span = width - x;
    val  = raw & 0xFFFF;
    switch (arg5) {
        case 2:
            max = 0x3C;
            break;

        case 3:
            max = 0x5F;
            break;

        case 1:
            max  = 0x14;
            arg2 = 0;
            break;

        default:
            max = val;
            break;
    }

    if (arg2 == 1) {
        prevId = arg1 - 1;
        {
            aPrev   = (prevId & 0x30) >> 4;
            bPrev   = (prevId & 0xC) >> 2;
            cPrev   = prevId & 3;
            rawPrev = Gp_IdParamHi.rows[(((aPrev * 3) + bPrev) * 3) + cPrev].field[arg5];
        }
        if (arg5 == 0) {
            if (Mc_SaveData[0].state.gameMode > 0) {
                rawPrev = (rawPrev * 4) / 5;
            } else if (Mc_SaveData[0].state.clearCount > 0) {
                rawPrev = (rawPrev * 2) / 5;
            }
        }
        color              = 0x606060;
        p                  = gGpuPrimCursor;
        p->y0              = (arg0->panel.field_22.u + arg4) - 0xC;
        prev               = rawPrev & 0xFFFF;
        loc.req.x          = arg0->panel.field_20.u + x;
        textY              = arg0->panel.field_22.u - 6;
        loc.req.y          = textY + arg4;
        gGpuPrimCursor     = p + 1;
        loc.req.otIndex    = (arg0->panel.field_14.s) + 1;
        loc.req.field_8    = color;
        loc.req.glyphTable = 0;
        loc.req.centerMode = 0;
        loc.req.field_E    = 3;
        Text_DrawString(&loc.req, Text_ItoaSigned(loc.buf, prev));
        if (prev < val) {
            s32 y;

            y = arg4 - 3;
            Ui_AllocTile(&(arg0)->panel, x, y, (span * prev) / max, 3, 0x1741FU);
            color = 0xD287F;
            Ui_LayoutWithMode1(arg0, x, y, ((span * val) / max), 3, 0x1A50FE);
            p->u0                 = 0xA0;
            PRIM_COLOR_WORD(p, 0) = color;
            p->y0                 = p->y0 - 1;
        } else {
            if (val < prev) {
                s32 y;

                y = arg4 - 3;
                Ui_AllocTile(&(arg0)->panel, x, y, (span * val) / max, 3, 0x1741FU);
                color = 0x1741F;
                Ui_LayoutWithMode1(arg0, x, y, ((span * prev) / max), 3, 1);
                p->u0                 = 0x30;
                PRIM_COLOR_WORD(p, 0) = color;
            } else {
                if (val > 0) {
                    Ui_LayoutWithMode1(arg0, x, (arg4 - 3), ((span * val) / max), 3, 0x1741F);
                }
                p->u0                 = 0x78;
                PRIM_COLOR_WORD(p, 0) = color;
            }
        }
        spriteX = arg0->panel.field_20.u + x;
        p->w    = 8;
        p->h    = 8;
        p->v0   = 0x60;
        p->clut = 0x3C09;
        setSprt(p);
        p->x0 = spriteX + 0x14;
        addPrim(gGpuCurrentOt + arg0->panel.field_14.s + 1, p);
        Ui_InsertDrawTPage((arg0->panel.field_14.s) + 1, 0);
        loc.req2.x          = (arg0->panel.field_20.u + 0x1E) + x;
        textY               = arg0->panel.field_22.u - 6;
        loc.req2.y          = textY + arg4;
        loc.req2.otIndex    = (arg0->panel.field_14.s) + 1;
        loc.req2.field_8    = color;
        loc.req2.glyphTable = 0;
        loc.req2.centerMode = 0;
        loc.req2.field_E    = 3;
        Text_DrawString(&loc.req2, Text_ItoaSigned(loc.buf, val));
    } else {
        if (arg5 == 1) {
            loc.req2.x          = arg0->panel.field_20.u + x;
            textY2              = arg0->panel.field_22.u - 6;
            loc.req2.y          = textY2 + arg4;
            loc.req2.otIndex    = (arg0->panel.field_14.s) + 1;
            loc.req2.field_8    = 0x606060;
            loc.req2.glyphTable = 0;
            loc.req2.centerMode = 0;
            loc.req2.field_E    = 3;
            text                = Text_ItoaSignedPlus(loc.buf, val);
        } else {
            loc.req2.x          = arg0->panel.field_20.u + x;
            textY2              = arg0->panel.field_22.u - 6;
            loc.req2.y          = textY2 + arg4;
            loc.req2.otIndex    = (arg0->panel.field_14.s) + 1;
            loc.req2.field_8    = 0x606060;
            loc.req2.glyphTable = 0;
            loc.req2.centerMode = 0;
            loc.req2.field_E    = 3;
            text                = Text_ItoaSigned(loc.buf, val);
        }
        Text_DrawString(&loc.req2, text);
        if (val > 0) {
            Ui_LayoutWithMode1(arg0, x, (arg4 - 3), ((span * val) / max), 3, 0x1741F);
        }
    }
}

static void func_800D3D98(UiObject* arg0, s32 arg1, s32 arg2)
{
    TextDrawReq req;
    TextDrawReq req2;
    TextDrawReq req3;
    s32         color;
    s32         color2;
    s32         x;
    s32         y;
    s32         mask;
    s32         line;
    s32         temp;
    u8*         text;

    if (arg2 == 1) {
        if ((arg1 & 3) == 0) {
            arg2 = 0;
        }
        arg1 += 1;
    }

    color = 0x606060;
    x     = arg0->panel.field_1C.s + 2;
    y     = (s16)arg0->panel.field_18.u + 0xF;
    mask  = arg1 & 3;
    Gp_DrawItemLabel(arg0, x, y, arg1, color, 0);
    if (mask != 0) {
        func_800C2538(arg0, x, y, mask, color);
    }

    text           = Gp_StrAreaEffect;
    x              = arg0->panel.field_1C.s + 2;
    line           = (s16)arg0->panel.field_18.u;
    y              = line + 0x21;
    req.x          = arg0->panel.field_20.u + x;
    req.y          = arg0->panel.field_22.u + line + 0x1C;
    req.otIndex    = arg0->panel.field_14.s + 1;
    req.field_8    = color;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    Text_DrawString(&req, text);

    if (CdCmd_IsIdle() & 0xFFFF) {
        func_800C7AE8(arg0, x, y, 0x200);
    } else {
        func_800C7AE8(arg0, x, y, 0x300);
    }

    color2          = 0x606060;
    text            = Gp_StrCastCost;
    x               = arg0->panel.field_1C.s + 0x54;
    temp            = (s16)arg0->panel.field_18.u;
    req2.x          = arg0->panel.field_20.u + 1 + x;
    req2.y          = arg0->panel.field_22.u + temp + 0x24;
    y               = temp + 0x36;
    req2.otIndex    = arg0->panel.field_14.s + 1;
    req2.field_8    = color2;
    req2.glyphTable = 0;
    req2.centerMode = 0;
    req2.field_E    = 1;
    Text_DrawString(&req2, text);
    func_800D3660(arg0, arg1, arg2, x, y, 2);

    req3.x          = arg0->panel.field_20.u + 1 + x;
    req3.y          = arg0->panel.field_22.u + temp + 0x46;
    req3.otIndex    = arg0->panel.field_14.s + 1;
    req3.field_8    = color2;
    req3.glyphTable = 0;
    req3.centerMode = 0;
    req3.field_E    = 1;
    Text_DrawString(&req3, Gp_StrAtpLoss);
    y = temp + 0x58;
    func_800D3660(arg0, arg1, arg2, x, y, 3);
}

void Gp_MapMenuListTask(Task* arg0)
{
    UiObject* obj;
    UiList*   menu;
    Task*     child;
    UiObject* childObj;
    s32       x;
    s32       y;
    s32       flag;

    obj           = arg0->spawnArg2.pointer;
    menu          = &D_8010F81C;
    obj->field_2E = 0;
    if (arg0->state == 0) {
        Ui_LayoutListPanel(menu, &(obj)->panel);
        x = 0x96 - ((s16)obj->panel.bounds.unsignedRect.x + (s16)obj->panel.bounds.unsignedRect.w);
        y = 0x6E - ((s16)obj->panel.bounds.unsignedRect.y + (s16)obj->panel.bounds.unsignedRect.h);
        if (x < 0) {
            obj->panel.bounds.unsignedRect.x += x;
        }
        if (y < 0) {
            obj->panel.bounds.unsignedRect.y += y;
        }
        arg0->state = arg0->state + 1;
    }
    Ui_UpdateListNoAnim(menu, obj);
    if (obj->panel.field_0.w == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
            obj->field_2E = -1;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(4, 0, 0);
            obj->field_2E = 6;
        }
    }
    child = arg0->firstChild;
    if (child != NULL) {
        childObj = child->spawnArg2.pointer;
        flag     = childObj->field_2E;
        if ((flag == -1) || (flag == 6)) {
            obj->field_2E = childObj->field_2E;
        }
    }
}

void Gp_MapScreenTask(Task* arg0)
{
    UiObject* obj;
    s32       one;

    obj = arg0->spawnArg2.pointer;
    if (arg0->state == 0) {
        one                     = 1;
        obj                     = Ui_SpawnFromDesc(&D_8010F840, arg0->spawnArg1, one, one, NULL);
        arg0->spawnArg2.pointer = obj;
        if (obj != NULL) {
            obj->panel.bounds.unsignedRect.x = (u16)D_80114E8C;
            obj->panel.bounds.unsignedRect.y = (u16)D_80114E90;
        }
        Stage_InitPrimBufOnce();
        GameMain_SetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
        gGameSession->uiOpen = 1;
        D_80114E88           = 0;
        arg0->state          = arg0->state + 1;
    } else if (arg0->state == 1) {
        if ((obj->field_2E == -1) || (obj->field_2E == 6)) {
            Ui_TeardownTree(obj, obj->owner);
            arg0->killCountdown = 0xA;
            arg0->state         = 2;
        }
    } else {
        arg0->killCountdown--;
        if (arg0->killCountdown <= 0) {
            GameMain_SetFrameTiming(DISPLAY_TIMING_TWO_VBLANKS);
            gGameSession->uiOpen = 0;
            taskKill(arg0);
            Stage_ReleasePrimBuf();
            Stage_SetEndingFlag();
        }
    }
}

static void func_800D4270(UiObject* obj, TmdSource* mesh, s32 mode, s32 dp)
{
    RECT              tw;
    DR_MODE*          dr;
    s32               otz;
    u8*               verts;
    GpMapMarkScratch* scratch;
    u32*              cur;
    s32               type;
    u32               word;
    s32               count;
    s32               stride;
    u16               vz;
    s32               minX;
    s32               minY;

    otz            = obj->panel.field_14.s;
    verts          = (u8*)mesh->verts;
    cur            = mesh->stream;
    tw.y           = 0;
    tw.x           = 0;
    scratch        = SCRATCH_PUSH(GpMapMarkScratch);
    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    tw.h           = 0xFF;
    tw.w           = 0xFF;
    setTexWindow(dr, &tw);
    addPrim(&gGpuCurrentOt[otz], dr);
    scratch->offX = 0;
    scratch->offY = 0;
    while (*cur != TMD_STREAM_PART_END) {
        type   = *cur;
        cur   += 2;
        word   = *cur;
        count  = word >> 16;
        stride = word & 0xFFFF;
        cur   += 1;
        if (type == 0x44) {
            if (count > 0) {
                do {
                    POLY_FT4* p4;
                    SVECTOR*  vert;

                    vert           = (SVECTOR*)(verts + (((u16*)cur)[0] & 0xFFF8));
                    p4             = gGpuPrimCursor;
                    gGpuPrimCursor = p4 + 1;
                    gte_lddp(dp);
                    gte_ldsv(vert);
                    gte_gpf12();
                    gte_stsv(scratch);
                    p4->x0 = scratch->vx + scratch->offX;
                    vz     = scratch->vz;
                    p4->y0 = scratch->offY - vz;

                    vert = (SVECTOR*)(verts + (((u16*)cur)[1] & 0xFFF8));
                    gte_lddp(dp);
                    gte_ldsv(vert);
                    gte_gpf12();
                    gte_stsv(scratch);
                    p4->x1 = scratch->vx + scratch->offX;
                    vz     = scratch->vz;
                    p4->y1 = scratch->offY - vz;

                    vert = (SVECTOR*)(verts + (((u16*)cur)[2] & 0xFFF8));
                    gte_lddp(dp);
                    gte_ldsv(vert);
                    gte_gpf12();
                    gte_stsv(scratch);
                    p4->x2 = scratch->vx + scratch->offX;
                    vz     = scratch->vz;
                    p4->y2 = scratch->offY - vz;

                    vert = (SVECTOR*)(verts + (((u16*)cur)[3] & 0xFFF8));
                    gte_lddp(dp);
                    gte_ldsv(vert);
                    gte_gpf12();
                    gte_stsv(scratch);
                    p4->x3 = scratch->vx + scratch->offX;
                    vz     = scratch->vz;
                    p4->y3 = scratch->offY - vz;
                    if (mode == 0) {
                        minX = p4->x0;
                        if (p4->x1 < minX) {
                            minX = p4->x1;
                        }
                        if (p4->x2 < minX) {
                            minX = p4->x2;
                        }
                        if (p4->x3 < minX) {
                            minX = p4->x3;
                        }
                        minY = p4->y0;
                        if (p4->y1 < minY) {
                            minY = p4->y1;
                        }
                        if (p4->y2 < minY) {
                            minY = p4->y2;
                        }
                        if (p4->y3 < minY) {
                            minY = p4->y3;
                        }
                        p4->clut = 0x3FC0;
                        p4->u0   = (minX & 0x1F) + ((u8)p4->x0 - minX);
                        p4->v0   = (minY & 0x1F) + ((u8)p4->y0 - minY);
                        p4->u1   = (minX & 0x1F) + ((u8)p4->x1 - minX);
                        p4->v1   = (minY & 0x1F) + ((u8)p4->y1 - minY);
                        p4->u2   = (minX & 0x1F) + ((u8)p4->x2 - minX);
                        p4->v2   = (minY & 0x1F) + ((u8)p4->y2 - minY);
                        p4->u3   = (minX & 0x1F) + ((u8)p4->x3 - minX);
                        p4->v3   = (minY & 0x1F) + ((u8)p4->y3 - minY);
                    } else {
                        p4->u0 = p4->x0 - 0x80;
                        p4->u1 = p4->x1 - 0x80;
                        p4->u2 = p4->x2 - 0x80;
                        p4->u3 = p4->x3 - 0x80;
                        p4->v0 = p4->y0 - 0x78;
                        p4->v1 = p4->y1 - 0x78;
                        p4->v2 = p4->y2 - 0x78;
                        p4->v3 = p4->y3 - 0x78;
                        if (mode == 1) {
                            PRIM_COLOR_WORD(p4, 0) = PRIM_RGBC(0x20, 0x20, 0x20, 0);
                        } else if (mode == 2) {
                            PRIM_COLOR_WORD(p4, 0) = PRIM_RGBC(0x40, 0x40, 0xff, 0);
                        } else {
                            PRIM_COLOR_WORD(p4, 0) = PRIM_RGBC(0xff, 0x40, 0x40, 0);
                        }
                        p4->clut = 0x4000;
                    }
                    p4->tpage = 0xAE;
                    setlen(p4, 9);
                    setcode(p4, 0x2C);
                    if (mode == 0) {
                        setcode(p4, 0x2D);
                        addPrim(&gGpuCurrentOt[otz], p4);
                    } else {
                        addPrim(&gGpuCurrentOt[otz] + 1, p4);
                    }
                    cur += stride;
                    count--;
                } while (count > 0);
            }
        } else if (type == 4) {
            if (count > 0) {
                do {
                    POLY_FT3* p3;
                    SVECTOR*  vert;

                    vert           = (SVECTOR*)(verts + (((u16*)cur)[0] & 0xFFF8));
                    p3             = gGpuPrimCursor;
                    gGpuPrimCursor = p3 + 1;
                    gte_lddp(dp);
                    gte_ldsv(vert);
                    gte_gpf12();
                    gte_stsv(scratch);

                    vert = (SVECTOR*)(verts + (((u16*)cur)[0] & 0xFFF8));
                    gte_lddp(dp);
                    gte_ldsv(vert);
                    gte_gpf12();
                    gte_stsv(scratch);
                    p3->x0 = scratch->vx + scratch->offX;
                    vz     = scratch->vz;
                    p3->y0 = scratch->offY - vz;

                    vert = (SVECTOR*)(verts + (((u16*)cur)[1] & 0xFFF8));
                    gte_lddp(dp);
                    gte_ldsv(vert);
                    gte_gpf12();
                    gte_stsv(scratch);
                    p3->x1 = scratch->vx + scratch->offX;
                    vz     = scratch->vz;
                    p3->y1 = scratch->offY - vz;

                    vert = (SVECTOR*)(verts + (((u16*)cur)[2] & 0xFFF8));
                    gte_lddp(dp);
                    gte_ldsv(vert);
                    gte_gpf12();
                    gte_stsv(scratch);
                    p3->x2 = scratch->vx + scratch->offX;
                    vz     = scratch->vz;
                    p3->y2 = scratch->offY - vz;
                    if (mode == 0) {
                        minX = p3->x0;
                        if (p3->x1 < minX) {
                            minX = p3->x1;
                        }
                        if (p3->x2 < minX) {
                            minX = p3->x2;
                        }
                        minY = p3->y0;
                        if (p3->y1 < minY) {
                            minY = p3->y1;
                        }
                        if (p3->y2 < minY) {
                            minY = p3->y2;
                        }
                        p3->clut = 0x3FC0;
                        p3->u0   = (minX & 0x1F) + ((u8)p3->x0 - minX);
                        p3->v0   = (minY & 0x1F) + ((u8)p3->y0 - minY);
                        p3->u1   = (minX & 0x1F) + ((u8)p3->x1 - minX);
                        p3->v1   = (minY & 0x1F) + ((u8)p3->y1 - minY);
                        p3->u2   = (minX & 0x1F) + ((u8)p3->x2 - minX);
                        p3->v2   = (minY & 0x1F) + ((u8)p3->y2 - minY);
                    } else {
                        p3->u0 = p3->x0 - 0x80;
                        p3->u1 = p3->x1 - 0x80;
                        p3->u2 = p3->x2 - 0x80;
                        p3->v0 = p3->y0 - 0x78;
                        p3->v1 = p3->y1 - 0x78;
                        p3->v2 = p3->y2 - 0x78;
                        if (mode == 1) {
                            PRIM_COLOR_WORD(p3, 0) = PRIM_RGBC(0x20, 0x20, 0x20, 0);
                        } else if (mode == 2) {
                            PRIM_COLOR_WORD(p3, 0) = PRIM_RGBC(0x40, 0x40, 0xff, 0);
                        } else {
                            PRIM_COLOR_WORD(p3, 0) = PRIM_RGBC(0xff, 0x40, 0x40, 0);
                        }
                        p3->clut = 0x4000;
                    }
                    p3->tpage = 0xAE;
                    setlen(p3, 7);
                    setcode(p3, 0x24);
                    if (mode == 0) {
                        setcode(p3, 0x25);
                        addPrim(&gGpuCurrentOt[otz], p3);
                    } else {
                        addPrim(&gGpuCurrentOt[otz] + 1, p3);
                    }
                    cur += stride;
                    count--;
                } while (count > 0);
            }
        }
    }
    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    tw.x           = 0;
    tw.y           = 0;
    tw.w           = 0x20;
    tw.h           = 0x20;
    setTexWindow(dr, &tw);
    addPrim(&gGpuCurrentOt[otz], dr);
    SCRATCH_POP(GpMapMarkScratch);
}

s32 func_800D4D2C(s32 arg0)
{
    s32 val;

    val                           = *(volatile s32*)&Mc_SaveData[0].state.at4.loc;
    *(volatile s32*)&Wip_UiHolder = 0;
    switch (val & ~0xFFFF) {
        case 0x1130000:
            Display_InitModeObj(&D_mist_parking_8018668C, arg0, 0, 0);
            break;
        case 0x21B0000:
            Display_InitModeObj(&D_dryfield_trailer_coach_80183F84, arg0, 0, 0);
            break;
        case 0x31B0000:
            Display_InitModeObj(&D_dryfield_night_trailer_coach_801846D0, arg0, 0, 0);
            break;
        case 0x3180000:
            Display_InitModeObj(&D_dryfield_night_garage_80181C2C, arg0, 0, 0);
            break;
        case 0x40D0000:
            Display_InitModeObj(&D_shelter_b1_armory_801824D0, arg0, 0, 0);
            break;
        case 0x4140000:
            Display_InitModeObj(&D_shelter_b1_underground_parking_801871F0.value, arg0, 0, 0);
            break;
        case 0x5040000:
            Display_InitModeObj(&D_shelter_1f_heliport_80181188, arg0, 0, 0);
            break;
        default:
            return 0;
    }
    return 1;
}

UiObject* Gp_SpawnItemPrompt(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    s32 one;

    one    = 1;
    arg3 <<= 16;
    return Ui_SpawnFromDesc(&D_8010F788, arg3 | arg1, one, one, arg0);
}

s32 func_800D4E78(s32 arg0, s32 arg1, s32 arg2)
{
    D_80114E94 = arg2;
    D_80114E8C = arg0;
    D_80114E90 = arg1;
    Display_InitModeObj(&D_8010F85C, arg2, 0, 0);
    return 1;
}

s32 func_800D4EC0(void)
{
    return D_80114E88;
}

void Gp_DrawUseAttachCmd(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;
    s32         status;
    s32         one;

    req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
    req.y          = arg1->panel.field_22.u + (u16)arg0->field_1A;
    req.otIndex    = arg1->panel.field_14.s + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 5;
    req.centerMode = 0;
    req.field_E    = 0;
    Text_DrawString(&req, Gp_StrUse2);

    status = arg1->panel.field_0.w;
    one    = 1;
    if (((status >> 16) == one) || (status == one)) {
        if (arg0->field_10 == arg0->field_8) {
            Ui_SetHolderParam(Gp_StrUseAttachHelp, 0, 0);
        }
    }

    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            arg1->field_2C = 6;
            arg1->field_2E = 6;
        }
    }
}

void Gp_DrawKeyItemCmd(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;
    s32         status;
    s32         one;

    req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
    req.y          = arg1->panel.field_22.u + (u16)arg0->field_1A;
    req.otIndex    = arg1->panel.field_14.s + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 5;
    req.centerMode = 0;
    req.field_E    = 0;
    Text_DrawString(&req, Gp_StrKeyItem2);

    status = arg1->panel.field_0.w;
    one    = 1;
    if (((status >> 16) == one) || (status == one)) {
        if (arg0->field_10 == arg0->field_8) {
            Ui_SetHolderParam(Gp_StrUseKeyHelp, 0, 0);
        }
    }

    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            arg1->field_2C = 8;
            arg1->field_2E = 6;
        }
    }
}

s32 func_800D50D4(s32 arg0, s32 arg1)
{
    s32 a;
    s32 b;
    s32 c;
    s32 val;

    a   = (arg0 & 0x30) >> 4;
    b   = (arg0 & 0xC) >> 2;
    c   = arg0 & 3;
    val = Gp_IdParamHi.rows[(a * 3 + b) * 3 + c].field[arg1];
    if (arg1 == 0) {
        if (Mc_SaveData[0].state.gameMode > 0) {
            val = (val * 4) / 5;
        } else if (Mc_SaveData[0].state.clearCount > 0) {
            val = (val * 2) / 5;
        }
    }
    return val & 0xFFFF;
}

void Gp_DrawPeSlotCmd(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;

    req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
    req.y          = arg1->panel.field_22.u + (u16)arg0->field_1A;
    req.otIndex    = arg1->panel.field_14.s + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    Text_DrawString(&req, Gp_StrCancel2);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            arg1->field_2E = 6;
        }
    }
}

void Gp_DrawMapCmd(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;
    s32         status;
    s32         one;

    req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
    req.y          = arg1->panel.field_22.u + (u16)arg0->field_1A;
    req.otIndex    = arg1->panel.field_14.s + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 5;
    req.centerMode = 0;
    req.field_E    = 0;
    Text_DrawString(&req, Gp_StrMap);

    status = arg1->panel.field_0.w;
    one    = 1;
    if (((status >> 16) == one) || (status == one)) {
        if (arg0->field_10 == arg0->field_8) {
            Ui_SetHolderParam(Gp_StrCheckMap, 0, 0);
        }
    }

    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            arg1->field_2C = 0x100;
            arg1->field_2E = 6;
        }
    }
}

void Gp_DrawDiscardCmd(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;

    req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
    req.y          = arg1->panel.field_22.u + (u16)arg0->field_1A;
    req.otIndex    = arg1->panel.field_14.s + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    Text_DrawString(&req, Gp_StrDiscard2);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            Ui_SpawnFromDesc(&D_8010F6FC, 0, 1, 1, arg1);
            arg1->panel.field_0.w = 0;
        }
    }
}

static void Gp_DrawExamineCmd(UiObject* arg0, Task* arg1, u8* arg2, s32 arg3)
{
    s32 one;

    if (arg1->state == 0) {
        Ui_SizeFromTextPlain(&(arg0)->panel, arg2);
        arg1->killCountdown = 0xBC;
        arg1->state         = arg1->state + 1;
    }

    one = 1;
    Text_DrawMultiLine(arg0, arg0->panel.field_1C.s + 2, (s16)arg0->panel.field_18.u + 0xF, arg2, arg3, one, 0);

    arg1->killCountdown--;
    if (arg0->panel.field_0.w == one) {
        if ((arg1->killCountdown <= 0) || (Pad_CheckButtons(0, one, Pad_MaskConfirm | Pad_MaskCancel) != 0)) {
            arg0->field_2E      = 6;
            arg1->killCountdown = 0x7FFF;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
            arg0->field_2E = -1;
        }
    }
}

static void Gp_DrawPushCmd(UiObject* arg0, Task* arg1)
{
    s32 one;
    u8* text;
    s32 color;

    color = 0x606060;
    text  = Gp_NoticeTexts[(u16)arg1->spawnArg1.value];

    if (arg1->state == 0) {
        Ui_SizeFromTextPlain(&(arg0)->panel, text);
        arg1->killCountdown = 0xBC;
        arg1->state         = arg1->state + 1;
    }

    one = 1;
    Text_DrawMultiLine(arg0, arg0->panel.field_1C.s + 2, (s16)arg0->panel.field_18.u + 0xF, text, color, one, 0);

    arg1->killCountdown--;
    if (arg0->panel.field_0.w == one) {
        if ((arg1->killCountdown <= 0) || (Pad_CheckButtons(0, one, Pad_MaskConfirm | Pad_MaskCancel) != 0)) {
            arg0->field_2E      = 6;
            arg1->killCountdown = 0x7FFF;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
            arg0->field_2E = -1;
        }
    }
}

void Gp_DrawNextLevelCmd(Task* arg0)
{
    UiObject* obj;
    s32       spawnArg;
    s32       saved;
    s32       y;
    u8*       text;

    obj                  = arg0->spawnArg2.pointer;
    spawnArg             = arg0->spawnArg1.value;
    saved                = obj->panel.field_0.w;
    obj->field_2E        = 0;
    obj->panel.field_0.w = 1;
    Ui_DrawText(&(obj)->panel, Gp_StrNextLevel);
    obj->panel.field_0.w = saved;
    func_800D3D98(obj, spawnArg, 1);
    y    = (s16)obj->panel.field_1A.u;
    text = Gp_GetItemText(spawnArg + 1, 1, 1);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, y - 0xF, text, 0x606060, 3, 0);
    text = Gp_GetItemText(spawnArg + 1, 2, 1);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, y, text, 0x606060, 3, 0);
}

void func_800D573C(Task* arg0)
{
    UiObject* obj;

    obj           = arg0->spawnArg2.pointer;
    obj->field_2E = 0;
    Gp_UseHealItemPanel(obj, arg0, arg0->spawnArg1.value);
}

void Gp_DrawSpecsCmd(Task* arg0)
{
    UiObject* obj;
    s32       spawnArg;
    u8*       text;

    obj           = arg0->spawnArg2.pointer;
    spawnArg      = arg0->spawnArg1.value;
    obj->field_2E = 0;
    Ui_DrawText(&(obj)->panel, Gp_StrSpecs2);
    if ((spawnArg & 3) == 0) {
        spawnArg += 1;
    }
    func_800D3D98(obj, spawnArg, 0);
    if (CdCmd_IsIdle() & 0xFFFF) {
        text = Text_SkipLines(Fs_GetChunkPayload(), 4);
        Text_DrawMultiLine(obj, obj->panel.field_1C.s + 2, 0x14, text, 0x606060, 3, 0);
    }
    if (obj->panel.field_0.w == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel | 0x10) != 0) {
            obj->field_2E = 6;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
            obj->field_2E = -1;
        }
    }
}

void Gp_DrawExaminePushCmd(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;
    char*       text;
    s32         one;
    s32         confirm;

    text = Gp_StrExamine;
    one  = 1;
    if (arg1->owner->spawnArg1.value == one) {
        text = Gp_StrPush;
    }
    req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
    req.y          = arg1->panel.field_22.u + (u16)arg0->field_1A;
    req.otIndex    = arg1->panel.field_14.s + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = one;
    Text_DrawString(&req, text);
    confirm = arg0->field_C;
    if (confirm == one) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            D_80114E88     = confirm;
            arg1->field_2E = 6;
        }
    }
}

void Gp_DrawItemCmd(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;

    req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
    req.y          = arg1->panel.field_22.u + (u16)arg0->field_1A;
    req.otIndex    = arg1->panel.field_14.s + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    Text_DrawString(&req, Gp_StrItem2);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            Ui_SpawnFromDesc(&D_8010EFBC, 0, 1, 1, arg1);
            SndEvt_EnqueueType6(3, 0, 0);
            Ui_SetState4(arg1, arg1->owner);
            arg1->panel.field_0.w = 0;
        }
    }
}

void func_800D5A48(Task* arg0)
{
    UiObject* obj;
    s32       flags;

    obj           = arg0->spawnArg2.pointer;
    obj->field_2E = 0;
    flags         = 0;
    if (arg0->state == 0) {
        if (arg0->spawnArg1.value == 0) {
            Ui_UpdateLayoutSize(&(obj)->panel, 0x84, 0x64);
        } else {
            Ui_UpdateLayoutSize(&(obj)->panel, 0x84, 0x83);
        }
        arg0->state = arg0->state + 1;
    }
    if (arg0->spawnArg1.value != 0) {
        flags |= 0x400;
    }
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        flags |= 0x100;
    }
    func_800C7AE8(obj, obj->panel.field_1C.s + 2, (s16)obj->panel.field_18.u + 2, flags);
}
