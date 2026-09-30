#include "gameplay/item_menu.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "gameplay/actor_render.h"
#include "gameplay/inventory.h"
#include "item_menu.h"
#include "item_use.h"
#include "gameplay/items.h"
#include "items.h"
#include "message.h"
#include "gameplay/starter_inventory.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

/// Selects stage, area and room-local view while ignoring the room byte.
#define GAME_LOCATION_STAGE_AREA_VIEW_MASK GAME_LOCATION_KEY(0xFF, 0xFF, 0, 0xFF)

#include "main/display.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"

/// 4 prompt strings copied onto the stack by `Gp_ItemMenuPrompt` and indexed
/// by `UiList::field_8`: All / Select / Discard / End
/// (`Gp_StrAll` / `Gp_StrSelect` / `Gp_StrDiscard` / `Gp_StrEnd`).
typedef struct {
    u8* texts[4];
} GpPromptTexts;
STATIC_ASSERT_SIZEOF(GpPromptTexts, 0x10);

/// 0x1C work block allocated by `Gp_ItemMoveTask` (`memCalloc(0x1C, 0)`)
/// and stored at `Task::work` / `Gp_ItemMoveWork`. `objs` holds the first two
/// `Ui_SpawnFromDesc` results (the source / dest inventory panes); `field_8` is
/// the index of the pane that currently has focus and is used to index `objs`
/// (`Gp_ItemMoveChild` toggles it with `^ 1`).
typedef struct _GpItemMoveState {
    /* 0x00 */ UiObject* objs[2];
    /* 0x08 */ s32       field_8;
    /* 0x0C */ s32       field_C;
    /* 0x10 */ s32       field_10;
    /* 0x14 */ s32       field_14;
    /* 0x18 */ s32       field_18;
} GpItemMoveState;
STATIC_ASSERT_SIZEOF(GpItemMoveState, 0x1C);

/// Ammo quantity selector work block, allocated by func_800BDF6C and stored
/// in Task::work. Equipped rounds stay in the destination inventory.
typedef struct _GpAmmoSplitState {
    /* 0x00 */ s32 srcOrig;
    /* 0x04 */ s32 dstOrig;
    /* 0x08 */ s32 srcQty;
    /* 0x0C */ s32 dstQty;
    /* 0x10 */ s32 equipped;
    /* 0x14 */ s32 limit;
} GpAmmoSplitState;
STATIC_ASSERT_SIZEOF(GpAmmoSplitState, 0x18);

GpItemMoveState* Gp_ItemMoveWork;

u16 Gp_MoveItemKey;

/// Popup spawned by `Gp_ItemMoveRow` on confirm when `owner->state == 1`.
#define D_8010D764 D_8010D6F4[4]

/// Quantity-selection popup opened by `func_800BD6DC` when moving ammo stacks.
#define D_8010D780 D_8010D6F4[5]

/// "Move items" confirmation popup spawned by `Gp_ItemMoveChild` when the
/// pane is closed with items still selected (`Gp_CanMoveItems` result as arg1).
#define D_8010D7F0 D_8010D6F4[9]

/// UiList used by `Gp_ItemActionListTask`.
extern UiList Gp_ItemActionList;

/// UiList used by `Gp_ItemMenuListTask`. `field_10` is 1 when `spawnArg1` is 0.
extern UiList Gp_ItemMenuList;

extern UiListItemFunc D_8010D6B0[1];

typedef struct {
    s32 id;
    s32 (*handler)(Task*, s32, GpCmdReply*);
} GpItemReplyEntry;

extern GpItemReplyEntry D_8010D828[2];

/* Kept next to Gp_ItemMoveChild's jump table so the overlay .rodata stays packed;
   Gp_StrBullet follows before func_800BDF6C. */
static const char Gp_StrBattleField[];

static const char Gp_StrItemBox[];

static const char Gp_StrPlayerItem[];

static const char Gp_StrBullet[];

/* After Gp_StrBullet from func_800BDF6C so the overlay .rodata stays packed. */
static const GpPromptTexts Gp_ItemPromptTexts;

/// Fullscreen-fade vector template used by `Gp_FadeTileTask` / `Gp_ItemPickupTilt`.
static const VECTOR D_80093DB0;

/// Task callback for the item-move UI. `spawnArg2` is the `UiObject`.
/// First run copies `Gp_ScanPtrs[Gp_PubItemLoc]` / `Mc_SaveData[0].state.carriedItems`
/// into `Gp_MoveScanSrc` / `Gp_MoveScanDst`, spawns the `D_8010D6F4` pair
/// (plus `[9]` when `spawnArg1 == 1`), then walks children through
/// `Gp_ItemMoveChild`. Always writes `field_2C = 0x34`.
void Gp_ItemMoveTask(Task* arg0);

/// Task callback for one `Gp_InvLists` inventory pane. `spawnArg1 >= 0x100`
/// is masked to the low byte and `flags` is set so the title is
/// `Gp_StrBattleField` ("Battle Field") instead of `Gp_StrItemBox` ("Item Box");
/// dest (`spawnArg1 != 0`) uses `Gp_StrPlayerItem` ("Player Item"). Seeds the
/// list from `Gp_MoveScanSrc[spawnArg1].rowCount` (visible rows capped at 10).
/// First-state confirm/cancel is `field_2E = -1`; later states write
/// `0x24`. Circle (src) / Square (dest) / mask 3 switch panes (`0xA`)
/// and play type-6 sound 2. Walks children through `Gp_CloseItemPane`.
void Gp_ItemPaneTask(Task* arg0);

/// Task callback for the `Gp_ItemActionList` item list. On first run it copies
/// `parent->flags`, clamps `field_E + field_12` to 0x64, then calls
/// `Gp_FillItemActions` and `Ui_LayoutListPanel`. Confirm (`Pad_MaskMenu`) is
/// cancel (`field_2E = -1`) when `owner->flags` is 0, else 6; cancel
/// (`Pad_MaskCancel`) is 6. Child `field_2E` -1 / 9 / 6 closes, remaps to
/// 6, or teardowns.
void Gp_ItemActionListTask(Task* arg0);

/// Ammo quantity selector. Adjusts source/destination stacks within the
/// stack limit, reserves equipped rounds, and applies the transfer on confirm.
void func_800BDF6C(Task* task);

/// List-item callback for All / Select / Discard / End. Draws
/// `Gp_ItemPromptTexts[field_8]`. Confirm: All → `field_2E = 0x26`, Select → 6,
/// Discard strips 0x80–0x9F attachments missing from
/// `Mc_SaveData[0].state.carriedItems` and sets `field_2E = 0x27`. Cancel once sets
/// `field_10 = 2` / `field_22 = 0x21`; a second cancel does the discard
/// strip.
void Gp_ItemMenuPrompt(UiList* arg0, UiObject* arg1);

void Gp_ItemMenuListTask(Task* arg0);

/// Task callback. `spawnArg2` is the `UiObject`; on first run it is published
/// as `Wip_UiHolder`. `spawnArg1` is a text pointer; when non-zero, two prompt
/// lines are drawn at `field_18 + 0xF` / `+ 0x1E` in color `0x606060`.
void Gp_HolderPromptTask(Task* arg0);

s32 Gp_BindItemObj2(Task* arg0, s32 arg1, GpCmdReply* arg2);

/// Per-child item-move handler. Walked by `Gp_ItemMoveTask` over
/// `obj->owner`'s children as `Gp_ItemMoveChild(child->spawnArg2.pointer, child)`.
static void Gp_ItemMoveChild(UiObject* arg0, Task* arg1);

/// The inventory scan an item pane lists; the pane's `spawnArg1` selects which
/// of the two side-by-side scans it shows.
static inline InventoryItemRange* _gpItemPaneScan(Task* task);

/// Fills `Gp_ItemActionFns` and `arg0->field_4` / `field_5` from the selected
/// inventory row (`Gp_MoveScanSrc[spawnArg1]` / `Gp_InvLists[spawnArg1].field_10`).
static void Gp_FillItemActions(UiList* arg0, UiObject* arg1);

/// For each carried weapon, empties the loaded ammunition and attachment
/// counts when the carried items no longer include any of that item.
static inline void _gpDropOrphanedWeaponLoads(void);

static void Gp_ForEachUiChild(UiObject* arg0, void (*arg1)(UiObject*, Task*));

static s32 Gp_ItemUseRestricted(s32 arg0, s32 arg1);

/// Child closer for the `Gp_InvLists` inventory panes. `-1` tears down and
/// either restores parent status or sets parent `field_2E = -1` when the
/// parent owner has no flags; `6` / `0x23` / `38` / `39` copy those codes
/// onto the parent (`6` also restores status).
static void Gp_CloseItemPane(UiObject* arg0, Task* arg1);

UiList         Gp_ItemActionList = { Gp_ItemActionFns, 3, { 3 }, 1, 10, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };
UiListItemFunc D_8010D6B0[1]     = { Gp_ItemMenuPrompt };
UiList         Gp_ItemMenuList   = { D_8010D6B0, 3, { 3 }, 0, 15, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };
UiObjectDesc   D_8010D6D8        = { -0x80000000, 0xFF9C, 0xFFE2, 200, 60, 36, 0, 0, 192, Gp_ItemMoveTask, 0 };
UiObjectDesc   D_8010D6F4[11]    = {
    { 0x80002, 0xFF70, 0xFF98, 144, 160, 56, 0, 0, 192, Gp_ItemPaneTask, 0 },
    { 0x80002, 0, 0xFF98, 144, 160, 52, 0, 0, 192, Gp_ItemPaneTask, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0 },
    { 0, 0, 0, 70, 46, 16, 0, 0, 192, Gp_ItemActionListTask, 0 },
    { 2, 0xFF9C, 0xFFE2, 200, 50, 8, 0, 0, 192, func_800BDF6C, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0 },
    { 0, 0xFFC6, 0xFFE2, 116, 60, 16, 0, 0, 192, Gp_ItemMenuListTask, 0 },
    { 0, 0xFF70, 64, 288, 40, 60, 0, 0, 192, Gp_HolderPromptTask, 0 },
};
GpItemReplyEntry D_8010D828[2] = { { 2011, Gp_BindItemObj2 }, { 0x7FFFFFFF, NULL } };

/// Per-child item-move handler. Walked by `Gp_ItemMoveTask` over
/// `obj->owner`'s children as `Gp_ItemMoveChild(child->spawnArg2.pointer, child)`.
static void Gp_ItemMoveChild(UiObject* arg0, Task* arg1)
{
    GpItemMoveState*    mem;
    UiObject*           obj;
    McItemRec*          tbl;
    InventoryItemRange* scanSrc;
    s32                 i;
    s32                 base;
    s32                 flag;
    s32                 val;
    InventoryItemRange* dst;
    InventoryItemRange* src;
    McItemRec*          recDst;
    McItemRec*          recSrc;
    s32                 rowDst;
    s32                 rowSrc;
    s32                 idDst;
    s32                 idSrc;
    s32                 qtyDst;
    s32                 qtySrc;
    InventoryItemRange* scan;
    McItemRec*          recA;
    McItemRec*          recB;
    s32                 rowA;
    s32                 rowB;
    s32                 idA;
    s32                 idB;
    s32                 qtyA;
    s32                 qtyB;
    s32                 subA;
    s32                 subB;

    obj = arg1->parent->spawnArg2.pointer;
    mem = (GpItemMoveState*)arg1->parent->work;
    switch (arg0->field_2E) {
        case 0x26:
            scanSrc = &Gp_MoveScanSrc;
            tbl     = Gp_GetItemTable(scanSrc);
            base    = scanSrc->firstRow;
            i       = 0;
            if (scanSrc->rowCount != 0) {
                do {
                    if (tbl[base].itemId != 0) {
                        Gp_GiveItem(&Gp_MoveScanDst, tbl[base].itemId, tbl[base].qty);
                        tbl[base].itemId = 0;
                        tbl[base].qty    = 0;
                    }
                    i++;
                    base++;
                } while (i < scanSrc->rowCount);
            }
            /* fallthrough */
        case -1:
            flag = 0;
            if (mem->objs[0]->owner->status != 0) {
                flag = Gp_CountScanItems(&Gp_MoveScanSrc) > 0;
            }
            mem->objs[mem->field_8]->owner->state     = 1;
            mem->objs[mem->field_8 ^ 1]->owner->state = 1;
            if ((arg0->field_2E != 0x26) && flag) {
                val = Gp_CanMoveItems();
                SndEvt_EnqueueType6(4, 0, 0);
                mem->field_8 = 0;
                Ui_SpawnFromDesc(&D_8010D7F0, val, 1, 1, mem->objs[0]);
                break;
            }
            /* fallthrough */
        case 0x27:
            gGameSession->uiOpen = 0;
            obj->field_2E        = -1;
            break;
        case 6:
            Ui_TeardownTree(arg0, arg1);
            mem->objs[mem->field_8]->owner->state     = 1;
            mem->objs[mem->field_8 ^ 1]->owner->state = 1;
            mem->field_8                              = mem->field_8 ^ 1;
            mem->objs[mem->field_8]->panel.field_0.w  = 0;
            mem->field_8                              = mem->field_8 ^ 1;
            mem->objs[mem->field_8]->panel.field_0.w  = 1;
            break;
        case 0x23:
            mem->field_10                             = mem->field_8;
            mem->field_14                             = Gp_InvLists[mem->field_8].field_10;
            mem->objs[mem->field_8]->owner->state     = 2;
            mem->objs[mem->field_8 ^ 1]->owner->state = 2;
            mem->objs[mem->field_8]->panel.field_0.w  = 0;
            mem->field_8                              = mem->field_8 ^ 1;
            mem->objs[mem->field_8]->panel.field_0.w  = 1;
            break;
        case 0x25:
            if (mem->field_10 != mem->field_8) {
                if (mem->field_8 == 0) {
                    rowSrc = mem->field_18;
                    rowDst = mem->field_14;
                } else {
                    rowSrc = mem->field_14;
                    rowDst = mem->field_18;
                }
                dst    = &Gp_MoveScanDst;
                recDst = Gp_GetScanSlot(dst, rowDst, 0);
                qtyDst = recDst->qty;
                idDst  = recDst->itemId;
                Gp_RemoveItem(dst, recDst, qtyDst);
                src    = dst - 1;
                recSrc = Gp_GetScanSlot(src, rowSrc, 0);
                qtySrc = recSrc->qty;
                idSrc  = recSrc->itemId;
                Gp_RemoveItem(src, recSrc, qtySrc);
                Gp_SetScanItem(dst, rowDst, idSrc, qtySrc);
                Gp_SetScanItem(src, rowSrc, idDst, qtyDst);
                if ((u8)(recDst->itemId + 0x80) < 0x20) {
                    Gp_ClearEquipSlot(recDst->itemId);
                }
            } else {
                rowA = mem->field_14;
                rowB = mem->field_18;
                if (rowA != rowB) {
                    scan = &Gp_MoveScanSrc + mem->field_10;
                    recA = Gp_GetScanSlot(scan, rowA, 0);
                    qtyA = recA->qty;
                    idA  = recA->itemId;
                    subA = recA->attachSlot;
                    Gp_RemoveItem(scan, recA, qtyA);
                    recB = Gp_GetScanSlot(scan, rowB, 0);
                    qtyB = recB->qty;
                    idB  = recB->itemId;
                    subB = recB->attachSlot;
                    Gp_RemoveItem(scan, recB, qtyB);
                    Gp_SetScanItem(scan, rowA, idB, qtyB)->attachSlot = subB;
                    Gp_SetScanItem(scan, rowB, idA, qtyA)->attachSlot = subA;
                }
            }
            mem->objs[mem->field_8]->owner->state     = 1;
            mem->objs[mem->field_8 ^ 1]->owner->state = 1;
            break;
        case 0x24:
            mem->objs[mem->field_8]->owner->state     = 1;
            mem->objs[mem->field_8 ^ 1]->owner->state = 1;
            if (mem->field_10 != mem->field_8) {
                mem->objs[mem->field_8]->panel.field_0.w = 0;
                mem->field_8                             = mem->field_8 ^ 1;
                mem->objs[mem->field_8]->panel.field_0.w = 1;
            }
            break;
        case 0xA:
            mem->objs[mem->field_8]->panel.field_0.w = 0;
            mem->field_8                             = mem->field_8 ^ 1;
            mem->objs[mem->field_8]->panel.field_0.w = 1;
            break;
    }
}

/* Kept next to Gp_ItemMoveChild's jump table so the overlay .rodata stays packed;
   Gp_StrBullet follows before func_800BDF6C. */
static const char Gp_StrBattleField[] = "Battle Field";
static const char Gp_StrItemBox[]     = "Item Box";
static const char Gp_StrPlayerItem[]  = "Player Item";

void Gp_ItemMoveTask(Task* arg0)
{
    UiObject*            obj;
    GpItemMoveState*     mem;
    s32                  i;
    InventoryItemRange*  src;
    InventoryItemRange** scans;
    u16                  item;
    s32                  val;
    s32                  code;
    Task*                owner;
    Task*                child;
    Task*                next;
    Task*                head;
    void                 (*cb)(UiObject*, Task*);

    obj           = arg0->spawnArg2.pointer;
    obj->field_2E = 0;
    if (arg0->state == 0) {
        Wip_UiHolder = NULL;
        D_80067634   = NULL;
        Gp_ClearPreviewItems();
        mem = memCalloc(0x1C, 0);
        i   = 0;
        if (mem == NULL) {
            code                 = -1;
            obj->field_2E        = code;
            code                 = 0x34;
            obj->panel.field_0.w = 0;
            goto end;
        }
        scans           = Gp_ScanPtrs;
        arg0->work      = (TaskIdMap*)mem;
        Gp_ItemMoveWork = mem;
        mem->field_C    = 0;
        do {
            if (i == 0) {
                item           = Gp_PubItemLoc;
                src            = scans[item & 0xFF];
                Gp_MoveItemKey = item;
            } else {
                src = &Mc_SaveData[0].state.carriedItems;
            }
            (&Gp_MoveScanSrc)[i] = *src;
            i++;
        } while (i < 2);
        Gp_SortItems(&Gp_MoveScanSrc, 0);
        if (arg0->spawnArg1.value == 1) {
            val          = Gp_CanMoveItems();
            mem->field_8 = 0;
            mem->objs[0] = Ui_SpawnFromDesc(D_8010D6F4, 0x100, 0, 1, obj);
            mem->objs[1] = Ui_SpawnFromDesc(D_8010D6F4 + 1, 0x101, 0, 1, obj);
            Ui_SpawnFromDesc(D_8010D6F4 + 9, val, 1, 1, mem->objs[0]);
        } else {
            mem->field_8         = 0;
            mem->objs[0]         = Ui_SpawnFromDesc(D_8010D6F4, 0, 1, 1, obj);
            mem->objs[1]         = Ui_SpawnFromDesc(D_8010D6F4 + 1, 1, 0, 1, obj);
            obj->panel.field_0.w = 0;
        }
        Ui_SpawnFromDesc(&D_8010D80C, 0, 0, 1, obj);
        gGameSession->uiOpen = 1;
        arg0->state          = arg0->state + 1;
    }

    cb    = Gp_ItemMoveChild;
    owner = obj->owner;
    child = owner->firstChild;
    if (child != NULL) {
        do {
            next = child->nextSibling;
            cb(child->spawnArg2.pointer, child);
            head  = owner->firstChild;
            child = next;
            if (head == NULL) {
                break;
            }
        } while (child != head);
    }
    code = 0x34;
end:
    obj->field_2C = code;
}

void Gp_ItemMoveRow(UiList* arg0, UiObject* arg1)
{
    McItemRec* rec;
    s32        item;
    s32        item2;
    s32        status;
    s32        flag;
    s32        flags;
    s32        idx;
    UiObject*  spawned;

    rec  = Gp_GetScanSlot(&Gp_MoveScanSrc + arg1->owner->spawnArg1.value, arg0->field_8, 0);
    item = rec->itemId;
    if (arg0->field_C != 1) {
        if ((arg1->owner->state != 1) && (arg0->field_8 == Gp_ItemMoveWork->field_14) &&
            (arg1->owner->spawnArg1.value == Gp_ItemMoveWork->field_10)) {
            arg0->field_1C = 0x37A78;
        }
    }
    status = arg1->panel.field_0.w;
    if (((status >> 16) == 1) || (status == 1)) {
        if (arg0->field_10 == arg0->field_8) {
            Gp_SetPreviewItem(item, 0);
            Gp_SetHolderItemText(item);
        }
    }
    if (arg1->owner->spawnArg1.value == 0) {
        Gp_DrawItemLabel(arg1, arg0->field_18, arg0->field_1A, item, arg0->field_1C, 0);
    } else if (rec->attachSlot <= 0) {
        Gp_DrawItemLabel(arg1, arg0->field_18, arg0->field_1A, item, arg0->field_1C, 1);
    } else {
        Gp_DrawItemLabel(arg1, arg0->field_18, arg0->field_1A, item, arg0->field_1C, 2);
    }
    if (item >= 0xA0 && item < 0xC0) {
        Gp_DrawQty(arg1, arg0->field_18, arg0->field_1A, rec->qty, arg0->field_1C);
    }
    if (arg0->field_C == 1) {
        Gp_SelItemRec = rec;
        if (arg1->owner->state == 1) {
            if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
                SndEvt_EnqueueType6(3, 0, 0);
                spawned = Ui_SpawnFromDesc(&D_8010D764, arg1->owner->spawnArg1, 1, 1, arg1);
                if (spawned != NULL) {
                    spawned->panel.bounds.unsignedRect.x = arg1->panel.field_20.u + (u16)arg1->panel.field_1C.s + 0x14;
                    spawned->panel.bounds.unsignedRect.y = (arg1->panel.field_22.u + (u16)arg0->field_1A) - 0x14;
                    arg1->panel.field_0.w                = 0;
                }
            } else if ((Pad_CheckButtons(0, 1, 0x10) != 0) && (item != 0)) {
                SndEvt_EnqueueType6(3, 0, 0);
                Ui_SpawnFromDesc(&D_8010EFA0, item, 1, 1, arg1);
                arg1->panel.field_0.w = 0;
            }
        } else if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            idx   = arg1->owner->spawnArg1.value;
            item2 = Gp_GetScanSlot(&Gp_MoveScanSrc + idx, Gp_InvLists[idx].field_10, 0)->itemId;
            SndEvt_EnqueueType6(3, 0, 0);
            item = -1;
            if (Gp_ItemMoveWork->field_10 != arg1->owner->spawnArg1.value) {
                flags = arg1->owner->status;
                flag  = 0;
                if (Gp_ItemDescs[item2].field_3 & 1) {
                    flag = flags == 1;
                }
                if ((Gp_MoveItemKey == 0x703) && (item2 == 0x81) && (Mc_SaveData[0].state.at4.loc.stage == 1)) {
                    flag = 1;
                }
                if (flag) {
                    item = 0x20;
                } else if ((item2 >= 0xA0 && item2 < 0xC0) && (arg1->owner->status == 0)) {
                    item = 8;
                } else if (arg1->owner->spawnArg1.value == 1) {
                    if ((item2 == Player_Status.weapon + 0x7F) || (item2 == Player_Status.armor + 0x5F)) {
                        item = 0xA;
                    }
                }
            }
            if (item >= 0) {
                Gp_SpawnItemPrompt(arg1, item, 0, 1);
                arg1->panel.field_0.w = 0;
            } else {
                Gp_ItemMoveWork->field_18 = arg0->field_8;
                arg1->field_2E            = 0x25;
            }
        }
    }
}

/// The inventory scan an item pane lists; the pane's `spawnArg1` selects which
/// of the two side-by-side scans it shows.
static inline InventoryItemRange* _gpItemPaneScan(Task* task)
{
    return &Gp_MoveScanSrc + task->spawnArg1.value;
}

void Gp_ItemPaneTask(Task* arg0)
{
    UiObject*           obj;
    UiList*             menu;
    InventoryItemRange* scan;
    s32                 count;
    s32                 n;
    s32                 status;
    Task*               owner;
    Task*               child;
    Task*               next;
    Task*               head;
    void                (*cb)(UiObject*, Task*);

    menu          = &Gp_InvLists[(u8)arg0->spawnArg1.value];
    obj           = arg0->spawnArg2.pointer;
    obj->field_2E = 0;
    if (arg0->state == 0) {
        if (arg0->spawnArg1.value >= 0x100) {
            arg0->spawnArg1.value = arg0->spawnArg1.value & 0xFF;
            arg0->status          = 1;
        } else {
            arg0->status = 0;
        }
        {
            s32 val;

            val             = _gpItemPaneScan(arg0)->rowCount;
            menu->field_4   = val;
            menu->field_5.u = val;
            if ((s8)val >= 0xB) {
                menu->field_5.u = 0xA;
            }
        }
        menu->field_10  = 0;
        menu->field_9.u = 0;
        Ui_LayoutListPanel(menu, &(obj)->panel);
        menu->field_A = 1;
        arg0->state   = arg0->state + 1;
    }

    if (arg0->spawnArg1.value == 0) {
        if (arg0->status == 1) {
            Ui_DrawText(&(obj)->panel, Gp_StrBattleField);
        } else {
            Ui_DrawText(&(obj)->panel, Gp_StrItemBox);
        }
    } else {
        Ui_DrawText(&(obj)->panel, Gp_StrPlayerItem);
    }
    Ui_ComputeVisibleRows(menu, &(obj)->panel);
    menu->field_A = 1;
    if (menu->field_10 >= menu->field_4) {
        menu->field_10 = menu->field_4 - 1;
    }
    n = menu->field_4;
    if ((s8)menu->field_5.u >= n) {
        menu->field_9.u = 0;
    }
    if (menu->field_4 != 0) {
        Ui_UpdateListNoAnim(menu, obj);
    }

    scan  = _gpItemPaneScan(arg0);
    count = scan->rowCount;
    count = count < Gp_CountScanItems(scan);
    if (count != 0) {
        obj->panel.field_4 |= 0x20000;
    } else {
        obj->panel.field_4 &= ~0x20000;
    }

    status = obj->panel.field_0.w;
    if (status == 1) {
        if (menu->field_4 == 0) {
            Ui_SmoothCursor(&(obj)->panel, obj->panel.field_1C.s + 4, (s16)obj->panel.field_18.u + 0xA);
        }
        if (arg0->state == status) {
            if (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
                obj->panel.field_0.w = 0;
                obj->field_2E        = -1;
            } else if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
                obj->panel.field_0.w = 0;
                obj->field_2E        = -1;
            } else if (Pad_CheckButtons(0, 0, 0x5000) == 0) {
                if (arg0->spawnArg1.value == 0) {
                    if (Pad_CheckButtons(0, 1, 0x2000) != 0) {
                        goto do_snd;
                    }
                }
                if (arg0->spawnArg1.value == status) {
                    if (Pad_CheckButtons(0, 1, 0x8000) != 0) {
                        goto do_snd;
                    }
                }
                if (Pad_CheckButtons(0, 1, 3) == 0) {
                    goto children;
                }
            do_snd:
                SndEvt_EnqueueType6(2, 0, 0);
                obj->field_2E = 0xA;
            }
        } else if (Pad_CheckButtons(0, 1, Pad_MaskCancel | Pad_MaskMenu) != 0) {
            obj->field_2E = 0x24;
        } else if (Pad_CheckButtons(0, 0, 0x5000) == 0) {
            if (arg0->spawnArg1.value == 0) {
                if (Pad_CheckButtons(0, 1, 0x2000) != 0) {
                    obj->field_2E = 0xA;
                    goto children;
                }
            }
            if (arg0->spawnArg1.value == status) {
                if (Pad_CheckButtons(0, 1, 0x8000) != 0) {
                    obj->field_2E = 0xA;
                    goto children;
                }
            }
            if (Pad_CheckButtons(0, 1, 3) != 0) {
                obj->field_2E = 0xA;
            }
        }
    }

children:
    cb    = Gp_CloseItemPane;
    owner = obj->owner;
    child = owner->firstChild;
    if (child != NULL) {
        do {
            next = child->nextSibling;
            cb(child->spawnArg2.pointer, child);
            head  = owner->firstChild;
            child = next;
            if (head == NULL) {
                break;
            }
        } while (child != head);
    }
}

void func_800BD6DC(UiList* arg0, UiObject* arg1)
{
    TextDrawReq req;
    s32         flags;
    McItemRec*  rec;
    Task*       scanOwner;
    Task*       owner;
    s32         idx;
    s32         selected;
    s32         restricted;
    s32         prompt;
    s32         chooseQty;
    s32         qty;
    s32         item;

    req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
    req.y          = arg1->panel.field_22.u + (u16)arg0->field_1A;
    req.otIndex    = arg1->panel.field_14.s + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    Text_DrawString(&req, Gp_StrMove2);
    selected = arg0->field_C;
    if ((selected == 1) && (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0)) {
        prompt    = -1;
        chooseQty = 0;
        idx       = arg1->owner->spawnArg1.value;
        rec       = Gp_GetScanSlot((&Gp_MoveScanSrc + (idx)), Gp_InvLists[idx].field_10, 0);
        item      = rec->itemId;
        qty       = rec->qty;
        SndEvt_EnqueueType6(3, 0, 0);
        if ((u32)(item - 0xA0) < 0x20U) {
            scanOwner = arg1->owner;
            if (scanOwner->status != 0) {
                if ((Gp_FindItemInScan(item, (&Gp_MoveScanSrc + (scanOwner->spawnArg1.value ^ 1))) == NULL) && (Gp_CanAddItem((&Gp_MoveScanSrc + (arg1->owner->spawnArg1.value ^ 1)), item) == 0)) {
                    prompt = 6;
                }
            } else if ((Gp_SumScanQty((&Gp_MoveScanSrc + (scanOwner->spawnArg1.value ^ 1)), item) != 0) || (Gp_CanAddItem((&Gp_MoveScanSrc + (arg1->owner->spawnArg1.value ^ 1)), item) != 0)) {
                chooseQty = 1;
            } else {
                prompt = 6;
            }
        } else if (Gp_CanAddItem((&Gp_MoveScanSrc + (arg1->owner->spawnArg1.value ^ 1)), item) != 0) {
            owner      = arg1->owner;
            flags      = owner->parent->status;
            restricted = 0;
            if (Gp_ItemDescs[item].field_3 & 1) {
                restricted = flags == 1;
            }
            if ((Gp_MoveItemKey == 0x703) && (item == 0x81) && (Mc_SaveData[0].state.at4.loc.stage == selected)) {
                restricted = 1;
            }
            if (restricted != 0) {
                prompt = 0x1E;
            } else if ((u32)(item - 0x80) < 0x20U) {
                if ((arg1->owner->spawnArg1.value != 1) || (item != (Player_Status.weapon + 0x7F))) {
                    if (prompt == -1) {
                        Gp_ClearEquipSlot(item);
                    }
                } else {
                    prompt = 7;
                }
            } else if (((u32)(item - 0x60) < 0x20U) && (arg1->owner->spawnArg1.value == 1) && (item == (Player_Status.armor + 0x5F))) {
                prompt = 7;
            }
        } else {
            prompt = 6;
        }
        if (prompt >= 0) {
            Gp_SpawnItemPrompt(arg1, prompt, 0, 0);
            arg1->panel.field_0.w = 0;
            return;
        }
        if (chooseQty == 1) {
            if (Ui_SpawnFromDesc(&D_8010D780, item, 1, 1, arg1) != NULL) {
                arg1->panel.field_0.w = 0;
            }
        } else {
            Gp_RemoveItem((&Gp_MoveScanSrc + (arg1->owner->spawnArg1.value)), rec, qty);
            Gp_GiveItem((&Gp_MoveScanSrc + (arg1->owner->spawnArg1.value ^ 1)), item, qty);
            arg1->field_2E = 6;
        }
    }
}

void Gp_ItemActionConfirm(UiList* arg0, UiObject* arg1)
{
    TextDrawReq   req;
    s32           selected;
    s32           idx;
    McItemRec*    rec;
    s32           item;
    s32           flag;
    s32           flags;
    Task*         owner;
    PlayerStatus* cfg;

    req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
    req.y          = arg1->panel.field_22.u + (u16)arg0->field_1A;
    req.otIndex    = arg1->panel.field_14.s + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    Text_DrawString(&req, Gp_StrSwitch);

    selected = arg0->field_C;
    if (selected == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            idx  = arg1->owner->spawnArg1.value;
            rec  = Gp_GetScanSlot(&Gp_MoveScanSrc + idx, Gp_InvLists[idx].field_10, 0);
            item = rec->itemId;
            SndEvt_EnqueueType6(3, 0, 0);

            owner = arg1->owner;
            flags = owner->parent->status;
            flag  = 0;
            if (Gp_ItemDescs[item].field_3 & 1) {
                flag = flags == 1;
            }
            if ((Gp_MoveItemKey == 0x703) && (item == 0x81) && (Mc_SaveData[0].state.at4.loc.stage == selected)) {
                flag = 1;
            }
            if (flag) {
                Gp_SpawnItemPrompt(arg1, 0x1E, 0, 0);
                arg1->panel.field_0.w = 0;
            } else if (arg1->owner->spawnArg1.value == 1) {
                cfg = &Player_Status;
                if ((item == cfg->weapon + 0x7F) || (item == cfg->armor + 0x5F)) {
                    Gp_SpawnItemPrompt(arg1, 7, 0, 0);
                    arg1->panel.field_0.w = 0;
                } else {
                    arg1->field_2E = 0x23;
                }
            } else {
                arg1->field_2E = 0x23;
            }
        }
    }
}

/// Fills `Gp_ItemActionFns` and `arg0->field_4` / `field_5` from the selected
/// inventory row (`Gp_MoveScanSrc[spawnArg1]` / `Gp_InvLists[spawnArg1].field_10`).
static void Gp_FillItemActions(UiList* arg0, UiObject* arg1)
{
    McItemRec*          rec;
    s32                 item;
    s32                 count;
    s32                 idx;
    UiListItemFunc*     table;
    Task*               owner;
    InventoryItemRange* scan;

    owner = arg1->owner;
    idx   = owner->spawnArg1.value;
    scan  = &Gp_MoveScanSrc + idx;
    rec   = Gp_GetScanSlot(scan, Gp_InvLists[idx].field_10, 0);
    item  = 0;
    if (rec != NULL) {
        item = rec->itemId;
    }
    if (item == 0) {
        Gp_ItemActionFns[0] = Gp_ItemActionConfirm;
        count               = 1;
    } else {
        Gp_ItemActionFns[0] = func_800BD6DC;
        table               = Gp_ItemActionFns;
        count               = 1;
        if (((u32)(item - 0xA0) >= 0x20U) || (arg1->owner->status != 0)) {
            table[1] = Gp_ItemActionConfirm;
            count    = 2;
        }
        if (((u32)(item - 1) < 3U) || (item == 5) || (item == 6) || (item == 7) || (item == 0x3C) || (item == 0x3D)) {
            Gp_ItemActionFns[count] = Gp_DrawUsePrompt;
            count                   = count + 1;
        }
    }
    arg0->field_5.u = count;
    arg0->field_4   = count;
}

void Gp_ItemActionListTask(Task* arg0)
{
    Task*     childTask;
    UiObject* obj;
    UiList*   menu;
    UiObject* child;
    s32       flag;
    Task*     parent;

    obj           = arg0->spawnArg2.pointer;
    obj->field_2E = 0;
    menu          = &Gp_ItemActionList;
    if (arg0->state == 0) {
        parent = arg0->parent;
        if (parent != NULL) {
            arg0->status = parent->status;
        }
        if (((s16)obj->panel.bounds.unsignedRect.y + (s16)obj->panel.bounds.unsignedRect.h) >= 0x65) {
            obj->panel.bounds.unsignedRect.y = 0x64 - obj->panel.bounds.unsignedRect.h;
        }
        Gp_FillItemActions(menu, obj);
        Ui_LayoutListPanel(menu, &(obj)->panel);
        arg0->state = arg0->state + 1;
    }
    Ui_UpdateListNoAnim(menu, obj);
    if (obj->panel.field_0.w == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
            if (obj->owner->status != 0) {
                obj->field_2E = 6;
            } else {
                obj->panel.field_0.w = 0;
                obj->field_2E        = -1;
            }
        } else if (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
            obj->field_2E = 6;
        }
    }
    childTask = arg0->firstChild;
    if (childTask != NULL) {
        child = childTask->spawnArg2.pointer;
        flag  = child->field_2E;
        switch (flag) {
            case -1:
                obj->panel.field_0.w = 0;
                obj->field_2E        = flag;
                break;
            case 9:
                obj->field_2E = 6;
                break;
            case 6:
                obj->panel.field_0.w = 1;
                Ui_TeardownTree(child, child->owner);
                break;
        }
    }
}

static const char Gp_StrBullet[] = "Bullet";

void func_800BDF6C(Task* task)
{
    u8                  buf[0x20];
    s32                 color;
    s32                 width;
    s32                 widthM2;
    s32                 half;
    InventoryItemRange* consumeScan;
    LINE_F2*            line;
    UiObject*           obj;
    s16                 panelY;
    s16                 coord;
    s32                 srcLimit;
    s32                 remaining;
    s32                 dstLimit;
    s32                 moveAllLimit;
    s32                 sourceQty;
    s32                 equippedWidth;
    s32                 textY;
    s32                 splitWidth;
    s32                 caretX;
    s32                 status;
    s32                 caretY;
    s32                 usableWidth;
    s32                 srcTotal;
    s32                 dstTotal;
    s32                 destAfterStep;
    s32                 negWidth;
    s32                 halfWidth;
    s32                 qty;
    s32                 totalQty;
    s32                 equipped;
    s32                 srcAfterMove;
    s32                 destAfterClamp;
    s32                 sourceToMove;
    s32                 combinedQty;
    s32                 destQty;
    s32                 repeatStep;
    s32                 transferQty;
    s32                 stepToSource;
    u8                  message;
    s16                 result;
    PadState*           pad;
    InventoryItemRange* sourceScan;
    InventoryItemRange* dstScan;
    GpAmmoSplitState*   state;

    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    width         = ((s16)obj->panel.field_1E.u - obj->panel.field_1C.s) - 0x50;
    Ui_DrawText(&(obj)->panel, (char*)Gp_StrBullet);
    if (task->state == 0) {
        state = (GpAmmoSplitState*)memCalloc(0x18U, 0);
        if (state == NULL) {
            obj->field_2E = 9;
            return;
        }
        task->work      = (TaskIdMap*)state;
        srcTotal        = Gp_ScanStackQty(&Gp_MoveScanSrc, task->spawnArg1.value);
        state->srcQty   = srcTotal;
        state->srcOrig  = srcTotal;
        dstTotal        = Gp_ScanStackQty(&Gp_MoveScanSrc + 1, task->spawnArg1.value);
        state->dstQty   = dstTotal;
        state->dstOrig  = dstTotal;
        state->equipped = Gp_CountEquippedRelated(&Gp_MoveScanSrc + 1, task->spawnArg1.value);
        Ui_SetHolderParam(Gp_StrSetAmmoHelp, 0, 0);
        state->limit = Gp_StackLimits[task->spawnArg1.value - 0xA0].maxHeld;
        task->state  = task->state + 1;
    }
    state = (GpAmmoSplitState*)task->work;
    Gp_DrawItemLabel(obj, obj->panel.field_1C.s + 2, (s16)obj->panel.field_18.u + 0xF, task->spawnArg1.value, 0x606060, 0);
    task->status = 0;
    totalQty     = state->srcQty + state->dstQty;
    color        = 0x606060;
    if (width < totalQty) {
        repeatStep = totalQty / width;
    } else {
        repeatStep = 1;
    }
    pad = Pad_States;
    if (pad->autoRepeat != 0) {
        pad->autoRepeat += gDisplayState.frameTicks * 2;
    }
    status = obj->panel.field_0.w;
    if (status == 1) {
        if (Pad_CheckButtons(0, 0, 0x5000) == 0) {
            if (Pad_CheckButtons(0, 1, 0x8000) != 0) {
                qty      = state->dstQty;
                equipped = state->equipped;
                if (equipped < qty) {
                    stepToSource = 1;
                    if ((u8)pad->autoRepeat >= 0x14U) {
                        stepToSource = repeatStep;
                    }
                    state->srcQty += stepToSource;
                    state->dstQty -= stepToSource;
                    if (state->dstQty < state->equipped) {
                        state->srcQty += state->dstQty - state->equipped;
                        state->dstQty  = state->equipped;
                    }
                    srcAfterMove = state->srcQty;
                    srcLimit     = state->limit;
                    if (srcLimit < srcAfterMove) {
                        state->srcQty = srcLimit;
                        state->dstQty = state->dstQty + (srcAfterMove - srcLimit);
                        goto step_at_capacity;
                    }
                } else if (equipped > 0) {
                    task->status = (u8)status;
                }
            } else if (Pad_CheckButtons(0, 1, 0x2000) != 0) {
                {
                    s32 step;

                    step = 1;
                    if ((u8)pad->autoRepeat >= 0x14U) {
                        step = repeatStep;
                    }
                    state->srcQty = state->srcQty - step;
                    destAfterStep = state->dstQty + step;
                    state->dstQty = destAfterStep;
                    remaining     = state->srcQty;
                    if (remaining < 0) {
                        state->dstQty = destAfterStep + remaining;
                        state->srcQty = 0;
                    }
                }
                destAfterClamp = state->dstQty;
                dstLimit       = state->limit;
                if (dstLimit < destAfterClamp) {
                    state->dstQty = dstLimit;
                    state->srcQty = state->srcQty + (destAfterClamp - dstLimit);
                step_at_capacity:
                    task->status = 2U;
                }
            }
        }
        if (Pad_CheckButtons(0, 0, 0xA000) == 0) {
            if (Pad_CheckButtons(0, 1, 0x1005) != 0) {
                if (state->dstQty > state->equipped) {
                    s32 total;

                    total  = state->srcQty + state->dstQty;
                    total -= state->equipped;
                    if (total < state->limit) {
                        state->dstQty = state->equipped;
                        state->srcQty = total;
                    } else {
                        state->srcQty = state->limit;
                        state->dstQty = state->equipped + (total - state->limit);
                        task->status  = 2;
                    }
                } else if (state->equipped > 0) {
                    task->status = 1;
                }
            } else if (Pad_CheckButtons(0, 1, 0x400A) != 0) {
                sourceToMove = state->srcQty;
                if (sourceToMove > 0) {
                    moveAllLimit = state->limit;
                    combinedQty  = sourceToMove + state->dstQty;
                    if (combinedQty < moveAllLimit) {
                        state->dstQty = combinedQty;
                        state->srcQty = 0;
                    } else {
                        state->dstQty = moveAllLimit;
                        state->srcQty = combinedQty - state->limit;
                        task->status  = 2U;
                    }
                }
            }
        }
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            transferQty = state->srcQty - state->srcOrig;
            if (transferQty > 0) {
                sourceScan = &Gp_MoveScanSrc;
                Gp_GiveItem(sourceScan, task->spawnArg1.value, transferQty);
                consumeScan = sourceScan + 1;
                goto consume_transfer;
            }
            result = 9;
            if (transferQty < 0) {
                transferQty = -transferQty;
                dstScan     = &Gp_MoveScanDst;
                Gp_GiveItem(dstScan, task->spawnArg1.value, transferQty);
                consumeScan = dstScan - 1;
            consume_transfer:
                Gp_ConsumeScanQty(consumeScan, task->spawnArg1.value, transferQty);
                result = 9;
            }
            goto set_result;
        }
        if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
            SndEvt_EnqueueType6(4, 0, 0);
            result               = -1;
            obj->panel.field_0.w = 0;
            goto set_result;
        }
        if (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(4, 0, 0);
            result = 9;
        set_result:
            obj->field_2E = result;
        }
    }
    destQty = state->dstQty;
    if ((destQty == state->equipped) && (destQty > 0)) {
        color = 0x37A78;
    }
    sourceQty   = state->srcQty;
    usableWidth = width - 2;
    widthM2     = usableWidth;
    panelY      = (s16)obj->panel.field_18.u;
    splitWidth  = ((s32)(sourceQty * usableWidth) / (s32)(sourceQty + state->dstQty)) + 1;
    textY       = panelY + 0x20;
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 0x20, textY, Text_ItoaUnsigned(buf, (u32)sourceQty), 0x606060, 1, 2);
    Text_DrawPrompt(obj, (s16)obj->panel.field_1E.u - 6, textY, Text_ItoaUnsigned(buf, (u32)state->dstQty), color, 1,
                    2);
    caretY    = panelY + 0x16;
    negWidth  = -width;
    halfWidth = (s32)(negWidth + ((u32)negWidth >> 0x1F)) >> 1;
    caretX    = halfWidth + splitWidth;
    half      = halfWidth;
    Ui_DrawFlatCaret(&(obj)->panel, caretX, caretY, 0x606060, 1);
    Ui_DrawFlatCaret(&(obj)->panel, caretX, panelY + 0x1E, 0x606060, 0);
    line                              = gGpuPrimCursor;
    gGpuPrimCursor                    = line + 1;
    GPU_PRIMITIVE_COLOR_WORD(line, 0) = PRIM_RGBC(0x60, 0x60, 0x60, 0);
    coord                             = obj->panel.field_20.u + caretX;
    line->x1                          = coord;
    line->x0                          = coord;
    line->y0                          = (obj->panel.field_22.u + textY) - 0xA;
    coord                             = (obj->panel.field_22.u + textY) - 2;
    setlen(line, 3);
    setcode(line, 0x40);
    line->y1 = coord;
    addPrim(gGpuCurrentOt + obj->panel.field_14.s + 1, line);
    qty = state->equipped;
    if (qty > 0) {
        equippedWidth = ((s32)(qty * widthM2) / (s32)(state->srcQty + state->dstQty)) + 2;
        if (equippedWidth > 0) {
            Ui_AllocTile(&(obj)->panel, (half + width) - equippedWidth, caretY, equippedWidth, 8, 0x37A78U);
        }
    }
    Ui_LayoutWithMode0(obj, (s32)-width / 2, textY - 0xA, width, 8, 0x102010);
    message = task->status;
    if (message == 1) {
        task->killCountdown = 0xBC;
        Ui_SetHolderParam(Gp_StrAmmoLocked, 0, 0);
    } else if (message == 2) {
        task->killCountdown = 0xBC;
        Ui_SetHolderParam(Gp_StrMaxCapacity, 0, 0);
    } else if (task->killCountdown > 0) {
        result              = (u16)task->killCountdown - 1;
        task->killCountdown = result;
        if ((result << 0x10) == 0) {
            Ui_SetHolderParam(Gp_StrSetAmmoHelp, 0, 0);
        }
    }
}

/* After Gp_StrBullet from func_800BDF6C so the overlay .rodata stays packed. */
static const GpPromptTexts Gp_ItemPromptTexts = { Gp_StrAll, Gp_StrSelect, Gp_StrDiscard, Gp_StrEnd };
/// Fullscreen-fade vector template used by `Gp_FadeTileTask` / `Gp_ItemPickupTilt`.
static const VECTOR D_80093DB0 = { 0, -100, 0, 0 };

/// For each carried weapon, empties the loaded ammunition and attachment
/// counts when the carried items no longer include any of that item.
static inline void _gpDropOrphanedWeaponLoads(void)
{
    InventoryItemRange* scan;
    McItemRec*          rec;
    McItemSlot*         slot;
    s32                 i;
    s32                 attach;

    scan = &Mc_SaveData[0].state.carriedItems;
    rec  = Gp_GetItemTable(scan);
    i    = 0;
    rec  = &rec[scan->firstRow];
    if (scan->rowCount != 0) {
        do {
            if ((u8)(rec->itemId + 0x80) < 0x20) {
                slot   = Gp_GetItemSlot(rec->itemId);
                attach = slot->ammoId;
                if ((attach != 0) && (attach != 0xB9)) {
                    if (Gp_SumScanQty(scan, attach) == 0) {
                        slot->ammoQty = 0;
                    }
                }
                attach = slot->attachId;
                if ((attach != 0) && (attach != 0xFF) && (attach != 0xB5) && (attach != 0xBB) &&
                    (attach != 0xBD) && (attach != 0xBE)) {
                    if (Gp_SumScanQty(scan, attach) == 0) {
                        slot->attachQty = 0;
                    }
                }
            }
            i++;
            rec++;
        } while (i < scan->rowCount);
    }
}

void Gp_ItemMenuPrompt(UiList* arg0, UiObject* arg1)
{
    GpPromptTexts texts;
    s32           mode;

    texts = Gp_ItemPromptTexts;
    if (arg0->field_8 == 0) {
        if (arg1->owner->spawnArg1.value == 0) {
            arg0->field_1C = Ui_LookupTable(arg1, 2);
            if (arg0->field_C == 1) {
                arg0->field_B  = 1;
                arg0->field_22 = 0x41;
                arg0->field_C  = 0;
            }
        }
    }
    mode = arg0->field_8;
    Text_DrawPrompt(arg1, arg0->field_18, arg0->field_1A, texts.texts[mode], arg0->field_1C, 1, 0);

    if (arg0->field_C == 1) {
        if (arg0->field_8 == 2) {
            if (arg0->field_22 == 0x21) {
                arg0->field_22 = 0;
                return;
            }
        }
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            switch (arg0->field_8) {
                case 0:
                    SndEvt_EnqueueType6(3, 0, 0);
                    arg1->field_2E = 0x26;
                    break;
                case 1:
                    SndEvt_EnqueueType6(3, 0, 0);
                    arg1->field_2E = 6;
                    break;
                case 2:
                    SndEvt_EnqueueType6(4, 0, 0);
                    _gpDropOrphanedWeaponLoads();
                    arg1->field_2E = 0x27;
                    break;
            }
        } else if (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(4, 0, 0);
            if (arg0->field_10 == 2) {
                _gpDropOrphanedWeaponLoads();
                arg1->field_2E = 0x27;
            } else {
                arg0->field_22 = 0x21;
                arg0->field_10 = 2;
            }
        }
    }
}

/// Task callback. `extra` is a `TmdObject`; `spawnArg2` is a `GpItemObj8`.
/// Tilts `coords[2]` (a `GfxCoord`) while playing a location-specific
/// type-6 sound, then signals `extraState` (`GpCmdReply.done = 1`) when
/// the motion returns to 0.
void Gp_ItemPickupTilt(Task* arg0)
{
    GameSession* session;
    TmdObject*   extra;
    GpItemObj8*  obj;
    GfxCoord*    coord;
    GfxCoord*    rot;
    VECTOR       vec;
    VECTOR       vec2;
    MATRIX*      mem;
    GpCmdReply*  done;
    u32          mapId;
    s32          room;
    s32          check;
    u16          item;

    extra   = arg0->extra.tmd;
    obj     = arg0->spawnArg2.pointer;
    session = gGameSession;
    mapId   = GAME_LOCATION_WORD(session->at4.loc) & GAME_LOCATION_STAGE_AREA_VIEW_MASK;
    item    = obj->field_A;
    coord   = extra->coords;
    rot     = coord + 2;
    room    = *&session->at4.loc.view;
    if (Gp_StateF0.field_4 == 2) {
        extra->flags |= TMD_OBJECT_HIDDEN;
    } else {
        extra->flags &= (u16)~TMD_OBJECT_HIDDEN;
    }
    mapId &= 0xFFFF0000;
    if (mapId == 0x4100000) {
        if ((u32)(room - 8) >= 2) {
            extra->flags |= TMD_OBJECT_HIDDEN;
        }
    } else if (mapId == 0x41F0000) {
        check = 3;
        goto compare_room;
    } else if (mapId == 0x4140000) {
        check = 0x11;
    compare_room:
        if (room != check) {
            extra->flags |= TMD_OBJECT_HIDDEN;
        }
    }
    if (arg0->state == 0) {
        mem = (MATRIX*)memCalloc(0x40, 0);
        if (mem != NULL) {
            vec             = D_80093DB0;
            extra->lightMtx = mem;
            extra->colorMtx = mem + 1;
            Gp_UpdateCoord(arg0->extra.tmd->coords);
            func_800D7A9C(extra, &vec, 0, 3);
            arg0->work = (TaskIdMap*)mem;
        }
        arg0->msgTable = D_8010D828;
        arg0->status   = 0;
        extra->flags   = 0;
        arg0->state++;
    } else if (arg0->state == 1) {
        if (Gp_GetCurBit2Flag(obj->field_8) != 2) {
            if (arg0->status != 0) {
                arg0->killCountdown = 0;
                switch (mapId) {
                    case 0x1060000: {
                        s32 temp;
                        temp = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
                        SndEvt_EnqueueType6(0x51060009, temp,
                                            (s8)gpGetObjDepth(arg0->extra.tmd->coords));
                        break;
                    }
                    case 0x10C0000: {
                        s32 temp;
                        temp = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
                        SndEvt_EnqueueType6(0x510C0005, temp,
                                            (s8)gpGetObjDepth(arg0->extra.tmd->coords));
                    }
                    case 0x21B0000: {
                        s32 temp;
                        temp = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
                        SndEvt_EnqueueType6(0x521B000B, temp,
                                            (s8)gpGetObjDepth(arg0->extra.tmd->coords));
                        break;
                    }
                    case 0x31B0000: {
                        s32 temp;
                        temp = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
                        SndEvt_EnqueueType6(0x531B000B, temp,
                                            (s8)gpGetObjDepth(arg0->extra.tmd->coords));
                        break;
                    }
                    case 0x4100000: {
                        s32 temp;
                        temp = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
                        SndEvt_EnqueueType6(0x54100012, temp,
                                            (s8)gpGetObjDepth(arg0->extra.tmd->coords));
                        break;
                    }
                    case 0x41F0000: {
                        s32 temp;
                        temp = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
                        SndEvt_EnqueueType6(0x541F0015, temp,
                                            (s8)gpGetObjDepth(arg0->extra.tmd->coords));
                        break;
                    }
                    case 0x4270000: {
                        s32 temp;
                        temp = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
                        SndEvt_EnqueueType6(0x54270008, temp,
                                            (s8)gpGetObjDepth(arg0->extra.tmd->coords));
                        break;
                    }
                }
                arg0->state++;
            }
        }
    } else if (arg0->state == 2) {
        arg0->killCountdown++;
        Gfx_RotMatrixX(&rot->coord, arg0->killCountdown << 5, 1);
        rot->composeStamp = GRAPHICS_COORD_DIRTY;
        if (arg0->killCountdown >= 0x14) {
            /* Unique items and stackables open the same pickup result task. */
            if (item < 0xA0) {
                Display_InitModeObj(Task_GetDesc(1, 0x26), 0, arg0->spawnArg2.value, 0);
            } else {
                Display_InitModeObj(Task_GetDesc(1, 0x26), 0, arg0->spawnArg2.value, 0);
            }
            arg0->state++;
        }
    } else if (arg0->state >= 3) {
        if (arg0->state == 3) {
            switch (mapId) {
                case 0x1060000: {
                    s32 temp;
                    temp = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
                    SndEvt_EnqueueType6(0x5106000A, temp,
                                        (s8)gpGetObjDepth(arg0->extra.tmd->coords));
                    break;
                }
                case 0x10C0000: {
                    s32 temp;
                    temp = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
                    SndEvt_EnqueueType6(0x510C0006, temp,
                                        (s8)gpGetObjDepth(arg0->extra.tmd->coords));
                    break;
                }
                case 0x21B0000:
                    break;
                case 0x31B0000: {
                    s32 temp;
                    temp = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
                    SndEvt_EnqueueType6(0x531B000C, temp,
                                        (s8)gpGetObjDepth(arg0->extra.tmd->coords));
                    break;
                }
                case 0x4100000: {
                    s32 temp;
                    temp = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
                    SndEvt_EnqueueType6(0x54100013, temp,
                                        (s8)gpGetObjDepth(arg0->extra.tmd->coords));
                    break;
                }
                case 0x41F0000: {
                    s32 temp;
                    temp = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
                    SndEvt_EnqueueType6(0x541F0016, temp,
                                        (s8)gpGetObjDepth(arg0->extra.tmd->coords));
                    break;
                }
                case 0x4270000: {
                    s32 temp;
                    temp = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
                    SndEvt_EnqueueType6(0x54270009, temp,
                                        (s8)gpGetObjDepth(arg0->extra.tmd->coords));
                    break;
                }
            }
            arg0->state++;
        }
        arg0->killCountdown -= 4;
        if (arg0->killCountdown <= 0) {
            arg0->killCountdown = 0;
        }
        Gfx_RotMatrixX(&rot->coord, arg0->killCountdown << 5, 1);
        rot->composeStamp = GRAPHICS_COORD_DIRTY;
        if (arg0->killCountdown == 0) {
            arg0->status = 0;
            done         = arg0->extraState.pointer;
            if (done != NULL) {
                done->done             = 1;
                arg0->extraState.value = 0;
            }
            arg0->state = 1;
        }
    }
    vec2 = D_80093DB0;
    Gp_UpdateCoord(arg0->extra.tmd->coords);
    func_800D7A9C(extra, &vec2, 0, 3);
}

static void Gp_ForEachUiChild(UiObject* arg0, void (*arg1)(UiObject*, Task*))
{
    Task* owner;
    Task* child;
    Task* next;
    Task* head;

    owner = arg0->owner;
    child = owner->firstChild;
    if (child != NULL) {
        do {
            next = child->nextSibling;
            arg1(child->spawnArg2.pointer, child);
            head  = owner->firstChild;
            child = next;
            if (head == NULL) {
                break;
            }
        } while (child != head);
    }
}

static s32 Gp_ItemUseRestricted(s32 arg0, s32 arg1)
{
    s32 ret;

    ret = 0;
    if (Gp_ItemDescs[arg0].field_3 & 1) {
        ret = arg1 == 1;
    }
    if ((Gp_MoveItemKey == 0x703) && (arg0 == 0x81) && (Mc_SaveData[0].state.at4.loc.stage == 1)) {
        ret = 1;
    }
    return ret;
}

/// Child closer for the `Gp_InvLists` inventory panes. `-1` tears down and
/// either restores parent status or sets parent `field_2E = -1` when the
/// parent owner has no flags; `6` / `0x23` / `38` / `39` copy those codes
/// onto the parent (`6` also restores status).
static void Gp_CloseItemPane(UiObject* arg0, Task* arg1)
{
    UiObject* parent;

    parent = arg1->parent->spawnArg2.pointer;
    switch (arg0->field_2E) {
        case -1:
            if (parent->owner->status) {
                parent->panel.field_0.w = 1;
                Ui_TeardownTree(arg0, arg0->owner);
            } else {
                Ui_TeardownTree(arg0, arg0->owner);
                parent->panel.field_0.w = 0;
                parent->field_2E        = -1;
            }
            break;
        case 6:
            parent->panel.field_0.w = 1;
            Ui_TeardownTree(arg0, arg0->owner);
            break;
        case 0x23:
            Ui_TeardownTree(arg0, arg0->owner);
            parent->field_2E = 0x23;
            break;
        case 38:
        case 39:
            parent->field_2E = arg0->field_2E;
            break;
    }
}

void Gp_ItemMenuListTask(Task* arg0)
{
    UiObject* obj;
    UiList*   menu;

    obj           = arg0->spawnArg2.pointer;
    obj->field_2E = 0;
    menu          = &Gp_ItemMenuList;
    if (arg0->state == 0) {
        Ui_LayoutListPanel(menu, &(obj)->panel);
        if (arg0->spawnArg1.value == 0) {
            menu->field_10 = 1;
        } else {
            menu->field_10 = 0;
        }
        menu->field_A = 1;
        arg0->state  += 1;
    }
    Ui_UpdateListNoAnim(menu, obj);
}

void Gp_HolderPromptTask(Task* arg0)
{
    UiObject* obj;
    u8*       val;
    s32       color;
    s32       one;
    u8*       text;

    obj           = arg0->spawnArg2.pointer;
    obj->field_2E = 0;
    if (arg0->state == 0) {
        Wip_UiHolder = obj;
        arg0->state += 1;
    }
    val = arg0->spawnArg1.pointer;
    if (val != 0) {
        color = 0x606060;
        one   = 1;
        Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, (s16)obj->panel.field_18.u + 0xF, val, color, one, 0);
        text = Text_SkipLines(val, one);
        Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, (s16)obj->panel.field_18.u + 0x1E, text, color, one, 0);
    }
}

s32 Gp_BindItemObj2(Task* arg0, s32 arg1, GpCmdReply* arg2)
{
    s32         flag;
    GpItemObj8* obj;

    obj                      = arg0->spawnArg2.pointer;
    flag                     = 1;
    arg0->status             = flag;
    arg0->extraState.pointer = arg2;
    if (Gp_GetCurBit2Flag(obj->field_8) == 2) {
        arg2->done = flag;
    }
    return 0;
}

void Gp_PublishItemObj(Task* arg0)
{
    GpItemObj8* obj = arg0->spawnArg2.pointer;
    s32         count;

    Gp_PubItemId  = obj->field_8;
    Gp_PubItemLoc = obj->field_A;
    if (obj->field_A < 0xA0) {
        if (Gp_PubItemLoc >= 0x60 && Gp_PubItemLoc < 0x80) {
            if (func_800B7420(Gp_PubItemLoc) != 0) {
                Gp_PubItemLoc = 0xD;
            }
        } else if (Gp_PubItemLoc >= 0x80 && Gp_PubItemLoc < 0xA0) {
            if (func_800B7420(Gp_PubItemLoc) != 0) {
                Gp_PubItemLoc = 0x3D;
            }
        }
        Gp_PubItemQty   = 1;
        Gp_PubItemReady = 1;
    } else {
        count           = Gp_StackLimits[Gp_PubItemLoc - 0xA0].perBuy;
        Gp_PubItemReady = 1;
        Gp_PubItemQty   = count;
    }
    GameMain_SetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
    Wip_UiHolder        = NULL;
    arg0->killCountdown = 1;
    arg0->state         = arg0->state + 1;
}

/// Fullscreen semi-trans TILE fade. `spawnArg1` 0/2 count down from 7/8;
/// 4 also counts down once `gDisplayState.control.flags.imageSource == 2`; 5 and other
/// values count up and write `gDisplayState.control.flags.imageSource` / `gDisplayState.control.flags.flipMode` on completion.
void Gp_FadeTileTask(Task* arg0)
{
    s32       flag;
    s32       yoff;
    s32       otIdx;
    s32       color;
    TILE*     tile;
    DR_TPAGE* dr;

    flag = 0;
    if (arg0->state == 0) {
        if (arg0->spawnArg1.value == 0) {
            GameMain_SetFrameTiming(DISPLAY_TIMING_TWO_VBLANKS);
            gDisplayState.control.flags.flipMode = DISPLAY_FLIP_FULL;
            arg0->killCountdown                  = 7;
        } else if ((arg0->spawnArg1.value == 2) || (arg0->spawnArg1.value == 4)) {
            arg0->killCountdown = 8;
        } else {
            arg0->killCountdown = 0;
        }
        arg0->state = arg0->state + 1;
    }

    if (arg0->spawnArg1.value == 4) {
        if (gDisplayState.control.flags.imageSource == DISPLAY_IMAGE_TRANSITION_STRIPS) {
            arg0->killCountdown--;
        } else {
            GameMain_SetFrameTiming(DISPLAY_TIMING_TWO_VBLANKS);
        }
    } else if ((arg0->spawnArg1.value == 0) || (arg0->spawnArg1.value == 2)) {
        flag = 0;
        arg0->killCountdown--;
    } else {
        flag                 = 1;
        arg0->killCountdown += flag;
    }

    yoff = 0;
    if (arg0->spawnArg1.value == 2) {
        yoff  = gDisplayState.vramYOffset;
        otIdx = 0;
    } else if (arg0->spawnArg1.value == 5) {
        otIdx = 0x3B;
    } else {
        otIdx = 0x3F;
    }

    tile           = gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    setlen(tile, 3);
    setcode(tile, 0x62);
    tile->x0 = -0xA0;
    tile->y0 = -0x78 - yoff;
    tile->w  = 0x140;
    tile->h  = 0xF0;
    if (arg0->killCountdown < 8) {
        color    = (arg0->killCountdown << 5) + 0x1F;
        tile->b0 = color;
        tile->g0 = color;
        tile->r0 = color;
    } else {
        color    = 0xFF;
        tile->b0 = color;
        tile->g0 = color;
        tile->r0 = color;
    }

    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((otIdx << 2)), tile);
    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE1000640;
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((otIdx << 2)), dr);

    if ((flag == 0) && (arg0->killCountdown <= 0)) {
        if (arg0->spawnArg1.value == 4) {
            GameMain_SetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
        }
        taskKill(arg0);
    } else if (flag == 1) {
        if (arg0->killCountdown >= 8) {
            if (arg0->spawnArg1.value == 5) {
                gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
            } else {
                gDisplayState.control.flags.flipMode = flag;
                GameMain_SetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
            }
            taskKill(arg0);
        }
    }
}
