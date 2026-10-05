#include "gameplay/item_menu.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "gameplay/actor_render.h"
#include "cap.h"
#include "gameplay/enemy.h"
#include "gameplay/inventory.h"
#include "item_menu.h"
#include "item_use.h"
#include "gameplay/items.h"
#include "items.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"
#include "gameplay/starter_inventory.h"
#include "gameplay/world_coords.h"

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

/// Prevents a cancel press from also activating the discard row it selected.
enum { ITEM_MENU_LIST_ACTION_CANCEL_HANDLED = 0x21 };

/// Row of the item-move command list.
///
/// The list counts three items, so End is stored and not dispatched. All is
/// not selectable when the destination cannot take every source row.
enum {
    ITEM_MENU_PROMPT_ALL     = 0, // Move every source row across, then leave.
    ITEM_MENU_PROMPT_SELECT  = 1, // Close the prompt and return to the inventory panes.
    ITEM_MENU_PROMPT_DISCARD = 2, // Clear ammunition whose item is no longer carried, then leave.
    ITEM_MENU_PROMPT_END     = 3, // Stored label; this row is not dispatched.
    ITEM_MENU_PROMPT_COUNT   = 4
};

/// Command labels for the item-move list, copied whole and indexed by the
/// list's current item.
///
/// Stored in row order: All, Select, Discard, End.
typedef struct {
    u8* label[ITEM_MENU_PROMPT_COUNT]; // Row text (0 All, 1 Select, 2 Discard, 3 End).
} _ItemMenuPromptTexts;
STATIC_ASSERT_SIZEOF(_ItemMenuPromptTexts, 0x10);

/// Work block of the item-move screen, which exchanges items between a
/// container and the items the player carries.
///
/// The screen shows two inventory panes side by side and gives input to one of
/// them at a time. A pane is identified throughout by its index, which is also
/// the index of the item range it lists: 0 is the container (the Item Box or
/// Battle Field pane), 1 the carried items (the Player Item pane).
///
/// The swap fields carry a row's Switch command across the two steps it takes:
/// choosing the command records the row and hands input to the other pane, and
/// confirming a second row there, or back in the first pane, exchanges the two.
/// They are meaningful only while that second row is being chosen.
///
/// Allocated on the screen task's first run and held in its `Task::work`.
typedef struct {
    UiObject* panes[2];       // The two inventory panes (0 container, 1 carried items).
    s32       focusedPane;    // Index in `panes` of the pane taking input.
    s32       field_C;        // Cleared when the screen opens and never read; role unproven.
    s32       swapPane;       // Pane holding the row Switch was chosen on.
    s32       swapRow;        // That row's item index in its pane's list.
    s32       swapPartnerRow; // Item index of the row confirmed to exchange with it.
} _ItemMenuMoveWork;
STATIC_ASSERT_SIZEOF(_ItemMenuMoveWork, 0x1C);

/// Work block of the ammunition quantity panel in the item-move screen.
///
/// The panel divides one ammunition item's rounds between the two item ranges
/// the screen exchanges: the container (the Item Box or Battle Field pane) and
/// the items the player carries. Every count is in rounds. The directional
/// buttons shift rounds between `containerQty` and `carriedQty`, whose sum
/// stays what it was when the panel opened; confirming moves the difference
/// between `containerQty` and `containerInitialQty` across.
///
/// Allocated on the panel task's first run and held in its `Task::work`.
typedef struct {
    s32 containerInitialQty; // Container's stack when the panel opened.
    s32 carriedInitialQty;   // Carried stack when the panel opened; stored and never read.
    s32 containerQty;        // Rounds the container would hold (0..`stackLimit`).
    s32 carriedQty;          // Rounds the player would carry (`loadedQty`..`stackLimit`).
    s32 loadedQty;           // Carried rounds loaded in carried weapons, which cannot be moved.
    s32 stackLimit;          // Largest quantity either side's stack may hold.
} _ItemMenuAmmoSplitWork;
STATIC_ASSERT_SIZEOF(_ItemMenuAmmoSplitWork, 0x18);

_ItemMenuMoveWork* Gp_ItemMoveWork;

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

extern UiListRowCallback D_8010D6B0[1];

extern TaskMessageEntry D_8010D828[2];

/* Kept next to Gp_ItemMoveChild's jump table so the overlay .rodata stays packed;
   Gp_StrBullet follows before func_800BDF6C. */
static const char Gp_StrBattleField[];

static const char Gp_StrItemBox[];

static const char Gp_StrPlayerItem[];

static const char Gp_StrBullet[];

/* After Gp_StrBullet from func_800BDF6C so the overlay .rodata stays packed. */
static const _ItemMenuPromptTexts Gp_ItemPromptTexts;

/// Fullscreen-fade vector template used by `Gp_FadeTileTask` / `Gp_ItemPickupTilt`.
static const VECTOR D_80093DB0;

/// Task callback for the item-move UI. `spawnArg2` is the `UiObject`.
/// First run copies `Gp_ScanPtrs[Gp_PubItemLoc]` / `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems`
/// into `Gp_MoveScanSrc` / `Gp_MoveScanDst`, spawns the `D_8010D6F4` pair
/// (plus `[9]` when `spawnArg1 == 1`), then walks children through
/// `Gp_ItemMoveChild`. Always writes `resultValue = 0x34`.
void Gp_ItemMoveTask(Task* arg0);

/// Task callback for one `Gp_InvLists` inventory pane. `spawnArg1 >= 0x100`
/// is masked to the low byte and `flags` is set so the title is
/// `Gp_StrBattleField` ("Battle Field") instead of `Gp_StrItemBox` ("Item Box");
/// dest (`spawnArg1 != 0`) uses `Gp_StrPlayerItem` ("Player Item"). Seeds the
/// list from `Gp_MoveScanSrc[spawnArg1].rowCount` (visible rows capped at 10).
/// First-state confirm/cancel is `result = USER_INTERFACE_RESULT_CANCEL`; later states write
/// `0x24`. Circle (src) / Square (dest) / mask 3 switch panes (`0xA`)
/// and play type-6 sound 2. Walks children through `Gp_CloseItemPane`.
void Gp_ItemPaneTask(Task* arg0);

/// Task callback for the `Gp_ItemActionList` item list. On first run it copies
/// `parent->flags`, clamps `field_E + field_12` to 0x64, then calls
/// `Gp_FillItemActions` and `Ui_LayoutListPanel`. Confirm (`Pad_MaskMenu`) is
/// cancel (`result = USER_INTERFACE_RESULT_CANCEL`) when `owner->flags` is 0, else
/// confirm; cancel (`Pad_MaskCancel`) is confirm. Child `result` of cancel, dismiss
/// or confirm closes, remaps dismiss to confirm, or tears the child down.
void Gp_ItemActionListTask(Task* arg0);

/// Ammo quantity selector. Adjusts source/destination stacks within the
/// stack limit, reserves equipped rounds, and applies the transfer on confirm.
void func_800BDF6C(Task* task);

/// List-item callback for the item-move commands. Draws the
/// `Gp_ItemPromptTexts` label for the current item. Confirm: All → `result = 0x26`, Select → confirm,
/// Discard zeroes loaded ammunition quantities whose item is absent from
/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems` and sets `result = 0x27`. Cancel once sets
/// `field_10 = 2` / `field_22 = 0x21`; a second cancel does the discard
/// strip.
void Gp_ItemMenuPrompt(UiList* arg0, UiObject* arg1);

void Gp_ItemMenuListTask(Task* arg0);

/// Task callback. `spawnArg2` is the `UiObject`; on first run it is published
/// as `Wip_UiHolder`. `spawnArg1` is a text pointer; when non-zero, two prompt
/// lines are drawn at `field_18 + 0xF` / `+ 0x1E` in color `0x606060`.
void Gp_HolderPromptTask(Task* arg0);

s32 Gp_BindItemObj2(Task* arg0, s32 arg1, CapActionRequest* request, s32 arg3);

/// Per-child item-move handler. Walked by `Gp_ItemMoveTask` over
/// `obj->owner`'s children as `Gp_ItemMoveChild(child->spawnArg2.pointer, child)`.
static void Gp_ItemMoveChild(UiObject* arg0, Task* arg1);

/// The inventory scan an item pane lists; the pane's `spawnArg1` selects which
/// of the two side-by-side scans it shows.
static inline InventoryItemRange* _gpItemPaneScan(Task* task);

/// Fills `Gp_ItemActionFns` and `arg0->field_4` / `field_5` from the selected
/// inventory row (`Gp_MoveScanSrc[spawnArg1]` / `Gp_InvLists[spawnArg1].field_10`).
static void Gp_FillItemActions(UiList* arg0, UiObject* arg1);

/// For each carried weapon, empties its primary and secondary ammunition
/// counts when the carried items no longer include any of that item.
static inline void _gpDropOrphanedWeaponLoads(void);

static void Gp_ForEachUiChild(UiObject* arg0, UiObjectTaskFunc arg1);

static s32 Gp_ItemUseRestricted(s32 arg0, s32 arg1);

/// Child closer for the `Gp_InvLists` inventory panes. Cancel tears down and
/// either restores parent status or sets the parent `result` to cancel when the
/// parent owner has no flags; confirm / `0x23` / `38` / `39` copy those codes
/// onto the parent (confirm also restores status).
static void Gp_CloseItemPane(UiObject* arg0, Task* arg1);

UiList            Gp_ItemActionList = { Gp_ItemActionFns, 3, { 3 }, 1, 10, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };
UiListRowCallback D_8010D6B0[1]     = { Gp_ItemMenuPrompt };
UiList            Gp_ItemMenuList   = { D_8010D6B0, 3, { 3 }, 0, 15, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };
UiObjectDesc      D_8010D6D8        = { (s32)USER_INTERFACE_PANEL_NO_FRAME, { -100, -30, 200, 60 }, 36, 0, TASK_BODY_NONE, 192, Gp_ItemMoveTask, 0 };
UiObjectDesc      D_8010D6F4[11]    = {
    { 0x80000 | USER_INTERFACE_PANEL_TITLE_STYLE, { -144, -104, 144, 160 }, 56, 0, TASK_BODY_NONE, 192, Gp_ItemPaneTask, 0 },
    { 0x80000 | USER_INTERFACE_PANEL_TITLE_STYLE, { 0, -104, 144, 160 }, 52, 0, TASK_BODY_NONE, 192, Gp_ItemPaneTask, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { 0, 0, 70, 46 }, 16, 0, TASK_BODY_NONE, 192, Gp_ItemActionListTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -100, -30, 200, 50 }, 8, 0, TASK_BODY_NONE, 192, func_800BDF6C, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { -58, -30, 116, 60 }, 16, 0, TASK_BODY_NONE, 192, Gp_ItemMenuListTask, 0 },
    { 0, { -144, 64, 288, 40 }, 60, 0, TASK_BODY_NONE, 192, Gp_HolderPromptTask, 0 },
};
TaskMessageEntry D_8010D828[2] = { { CAP_ACTION_MESSAGE_REQUEST, Gp_BindItemObj2 }, { TASK_MESSAGE_TABLE_END, NULL } };

/// Per-child item-move handler. Walked by `Gp_ItemMoveTask` over
/// `obj->owner`'s children as `Gp_ItemMoveChild(child->spawnArg2.pointer, child)`.
static void Gp_ItemMoveChild(UiObject* arg0, Task* arg1)
{
    _ItemMenuMoveWork*  work;
    UiObject*           obj;
    InventoryItemRow*   tbl;
    InventoryItemRange* scanSrc;
    s32                 i;
    s32                 base;
    s32                 flag;
    s32                 val;
    InventoryItemRange* dst;
    InventoryItemRange* src;
    InventoryItemRow*   recDst;
    InventoryItemRow*   recSrc;
    s32                 rowDst;
    s32                 rowSrc;
    s32                 idDst;
    s32                 idSrc;
    s32                 qtyDst;
    s32                 qtySrc;
    InventoryItemRange* scan;
    InventoryItemRow*   recA;
    InventoryItemRow*   recB;
    s32                 rowA;
    s32                 rowB;
    s32                 idA;
    s32                 idB;
    s32                 qtyA;
    s32                 qtyB;
    s32                 attachmentSlotA;
    s32                 attachmentSlotB;

    obj  = arg1->parent->spawnArg2.pointer;
    work = arg1->parent->work;
    switch (arg0->result) {
        case 0x26:
            scanSrc = &Gp_MoveScanSrc;
            tbl     = Gp_GetItemTable(scanSrc);
            base    = scanSrc->firstRow;
            i       = 0;
            if (scanSrc->rowCount != 0) {
                do {
                    if (tbl[base].itemId != INVENTORY_ITEM_NONE) {
                        Gp_GiveItem(&Gp_MoveScanDst, tbl[base].itemId, tbl[base].qty);
                        tbl[base].itemId = INVENTORY_ITEM_NONE;
                        tbl[base].qty    = 0;
                    }
                    i++;
                    base++;
                } while (i < scanSrc->rowCount);
            }
            /* fallthrough */
        case USER_INTERFACE_RESULT_CANCEL:
            flag = 0;
            if (work->panes[0]->owner->status != 0) {
                flag = Gp_CountScanItems(&Gp_MoveScanSrc) > 0;
            }
            work->panes[work->focusedPane]->owner->state     = 1;
            work->panes[work->focusedPane ^ 1]->owner->state = 1;
            if ((arg0->result != 0x26) && flag) {
                val = Gp_CanMoveItems();
                sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
                work->focusedPane = 0;
                Ui_SpawnFromDesc(&D_8010D7F0, val, 1, 1, work->panes[0]);
                break;
            }
            /* fallthrough */
        case 0x27:
            gGameSession->uiOpen = 0;
            obj->result          = USER_INTERFACE_RESULT_CANCEL;
            break;
        case USER_INTERFACE_RESULT_CONFIRM:
            Ui_TeardownTree(arg0, arg1);
            work->panes[work->focusedPane]->owner->state       = 1;
            work->panes[work->focusedPane ^ 1]->owner->state   = 1;
            work->focusedPane                                  = work->focusedPane ^ 1;
            work->panes[work->focusedPane]->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            work->focusedPane                                  = work->focusedPane ^ 1;
            work->panes[work->focusedPane]->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
            break;
        case 0x23:
            work->swapPane                                     = work->focusedPane;
            work->swapRow                                      = Gp_InvLists[work->focusedPane].selectedItemIndex;
            work->panes[work->focusedPane]->owner->state       = 2;
            work->panes[work->focusedPane ^ 1]->owner->state   = 2;
            work->panes[work->focusedPane]->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            work->focusedPane                                  = work->focusedPane ^ 1;
            work->panes[work->focusedPane]->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
            break;
        case 0x25:
            if (work->swapPane != work->focusedPane) {
                if (work->focusedPane == 0) {
                    rowSrc = work->swapPartnerRow;
                    rowDst = work->swapRow;
                } else {
                    rowSrc = work->swapRow;
                    rowDst = work->swapPartnerRow;
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
                rowA = work->swapRow;
                rowB = work->swapPartnerRow;
                if (rowA != rowB) {
                    scan            = &Gp_MoveScanSrc + work->swapPane;
                    recA            = Gp_GetScanSlot(scan, rowA, 0);
                    qtyA            = recA->qty;
                    idA             = recA->itemId;
                    attachmentSlotA = recA->attachSlot;
                    Gp_RemoveItem(scan, recA, qtyA);
                    recB            = Gp_GetScanSlot(scan, rowB, 0);
                    qtyB            = recB->qty;
                    idB             = recB->itemId;
                    attachmentSlotB = recB->attachSlot;
                    Gp_RemoveItem(scan, recB, qtyB);
                    Gp_SetScanItem(scan, rowA, idB, qtyB)->attachSlot = attachmentSlotB;
                    Gp_SetScanItem(scan, rowB, idA, qtyA)->attachSlot = attachmentSlotA;
                }
            }
            work->panes[work->focusedPane]->owner->state     = 1;
            work->panes[work->focusedPane ^ 1]->owner->state = 1;
            break;
        case 0x24:
            work->panes[work->focusedPane]->owner->state     = 1;
            work->panes[work->focusedPane ^ 1]->owner->state = 1;
            if (work->swapPane != work->focusedPane) {
                work->panes[work->focusedPane]->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                work->focusedPane                                  = work->focusedPane ^ 1;
                work->panes[work->focusedPane]->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
            }
            break;
        case 0xA:
            work->panes[work->focusedPane]->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            work->focusedPane                                  = work->focusedPane ^ 1;
            work->panes[work->focusedPane]->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
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
    _ItemMenuMoveWork*   work;
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
    UiObjectTaskFunc     cb;

    obj         = arg0->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    if (arg0->state == 0) {
        Wip_UiHolder = NULL;
        D_80067634   = NULL;
        Gp_ClearPreviewItems();
        work = memCalloc(sizeof(*work), 0);
        i    = 0;
        if (work == NULL) {
            code                    = USER_INTERFACE_RESULT_CANCEL;
            obj->result             = code;
            code                    = 0x34;
            obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            goto end;
        }
        scans           = Gp_ScanPtrs;
        arg0->work      = work;
        Gp_ItemMoveWork = work;
        work->field_C   = 0;
        do {
            if (i == 0) {
                item           = Gp_PubItemLoc;
                src            = scans[item & 0xFF];
                Gp_MoveItemKey = item;
            } else {
                src = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
            }
            (&Gp_MoveScanSrc)[i] = *src;
            i++;
        } while (i < 2);
        Gp_SortItems(&Gp_MoveScanSrc, 0);
        if (arg0->spawnArg1.value == 1) {
            val               = Gp_CanMoveItems();
            work->focusedPane = 0;
            work->panes[0]    = Ui_SpawnFromDesc(D_8010D6F4, 0x100, 0, 1, obj);
            work->panes[1]    = Ui_SpawnFromDesc(D_8010D6F4 + 1, 0x101, 0, 1, obj);
            Ui_SpawnFromDesc(D_8010D6F4 + 9, val, 1, 1, work->panes[0]);
        } else {
            work->focusedPane       = 0;
            work->panes[0]          = Ui_SpawnFromDesc(D_8010D6F4, 0, 1, 1, obj);
            work->panes[1]          = Ui_SpawnFromDesc(D_8010D6F4 + 1, 1, 0, 1, obj);
            obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
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
    obj->resultValue = code;
}

void Gp_ItemMoveRow(UiList* arg0, UiObject* arg1)
{
    InventoryItemRow* rec;
    s32               item;
    s32               item2;
    s32               status;
    s32               flag;
    s32               flags;
    s32               idx;
    UiObject*         spawned;

    rec  = Gp_GetScanSlot(&Gp_MoveScanSrc + arg1->owner->spawnArg1.value, arg0->currentItemIndex, 0);
    item = rec->itemId;
    if (arg0->rowInputEnabled != USER_INTERFACE_LIST_ROW_ACTIVE) {
        if ((arg1->owner->state != 1) && (arg0->currentItemIndex == Gp_ItemMoveWork->swapRow) &&
            (arg1->owner->spawnArg1.value == Gp_ItemMoveWork->swapPane)) {
            arg0->colorRgb = 0x37A78;
        }
    }
    status = arg1->panel.control.word;
    if (((status >> 16) == 1) || (status == 1)) {
        if (arg0->selectedItemIndex == arg0->currentItemIndex) {
            Gp_SetPreviewItem(item, 0);
            Gp_SetHolderItemText(item);
        }
    }
    if (arg1->owner->spawnArg1.value == 0) {
        Gp_DrawItemLabel(arg1, arg0->rowTextX.signedValue, arg0->rowTextY.signedValue, item, arg0->colorRgb, 0);
    } else if (rec->attachSlot <= INVENTORY_ATTACHMENT_NONE) {
        Gp_DrawItemLabel(arg1, arg0->rowTextX.signedValue, arg0->rowTextY.signedValue, item, arg0->colorRgb, 1);
    } else {
        Gp_DrawItemLabel(arg1, arg0->rowTextX.signedValue, arg0->rowTextY.signedValue, item, arg0->colorRgb, 2);
    }
    if (item >= 0xA0 && item < 0xC0) {
        Gp_DrawQty(arg1, arg0->rowTextX.signedValue, arg0->rowTextY.signedValue, rec->qty, arg0->colorRgb);
    }
    if (arg0->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        Gp_SelItemRec = rec;
        if (arg1->owner->state == 1) {
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
                sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                spawned = Ui_SpawnFromDesc(&D_8010D764, arg1->owner->spawnArg1, 1, 1, arg1);
                if (spawned != NULL) {
                    spawned->panel.bounds.unsignedRect.x = arg1->panel.contentOriginX.unsignedValue + arg1->panel.contentLeft.unsignedValue + 0x14;
                    spawned->panel.bounds.unsignedRect.y = (arg1->panel.contentOriginY.unsignedValue + arg0->rowTextY.unsignedValue) - 0x14;
                    arg1->panel.control.word             = USER_INTERFACE_PANEL_INACTIVE;
                }
            } else if ((padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_TRIANGLE) != 0) && (item != 0)) {
                sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                Ui_SpawnFromDesc(&D_8010EFA0, item, 1, 1, arg1);
                arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            idx   = arg1->owner->spawnArg1.value;
            item2 = Gp_GetScanSlot(&Gp_MoveScanSrc + idx, Gp_InvLists[idx].selectedItemIndex, 0)->itemId;
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            item = -1;
            if (Gp_ItemMoveWork->swapPane != arg1->owner->spawnArg1.value) {
                flags = arg1->owner->status;
                flag  = 0;
                if (Gp_ItemDescs[item2].flags & ITEM_FLAG_NO_DISCARD) {
                    flag = flags == 1;
                }
                if ((Gp_MoveItemKey == 0x703) && (item2 == 0x81) && (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage == GAME_STAGE_ACROPOLIS)) {
                    flag = 1;
                }
                if (flag) {
                    item = 0x20;
                } else if ((item2 >= 0xA0 && item2 < 0xC0) && (arg1->owner->status == 0)) {
                    item = 8;
                } else if (arg1->owner->spawnArg1.value == 1) {
                    if ((item2 == gPlayerStatus.weapon + 0x7F) || (item2 == gPlayerStatus.armor + 0x5F)) {
                        item = 0xA;
                    }
                }
            }
            if (item >= 0) {
                Gp_SpawnItemPrompt(arg1, item, 0, 1);
                arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            } else {
                Gp_ItemMoveWork->swapPartnerRow = arg0->currentItemIndex;
                arg1->result                    = 0x25;
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
    UiObjectTaskFunc    cb;

    menu        = &Gp_InvLists[(u8)arg0->spawnArg1.value];
    obj         = arg0->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    if (arg0->state == 0) {
        if (arg0->spawnArg1.value >= 0x100) {
            arg0->spawnArg1.value = arg0->spawnArg1.value & 0xFF;
            arg0->status          = 1;
        } else {
            arg0->status = 0;
        }
        {
            s32 val;

            val                                 = _gpItemPaneScan(arg0)->rowCount;
            menu->itemCount                     = val;
            menu->visibleRowCount.unsignedValue = val;
            if ((s8)val >= 0xB) {
                menu->visibleRowCount.unsignedValue = 0xA;
            }
        }
        menu->selectedItemIndex                   = 0;
        menu->firstVisibleItemIndex.unsignedValue = 0;
        Ui_LayoutListPanel(menu, &(obj)->panel);
        menu->flags = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        arg0->state = arg0->state + 1;
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
    menu->flags = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
    if (menu->selectedItemIndex >= menu->itemCount) {
        menu->selectedItemIndex = menu->itemCount - 1;
    }
    n = menu->itemCount;
    if (menu->visibleRowCount.signedValue >= n) {
        menu->firstVisibleItemIndex.unsignedValue = 0;
    }
    if (menu->itemCount != 0) {
        Ui_UpdateListNoAnim(menu, obj);
    }

    scan  = _gpItemPaneScan(arg0);
    count = scan->rowCount;
    count = count < Gp_CountScanItems(scan);
    if (count != 0) {
        obj->panel.style |= USER_INTERFACE_PANEL_SCREEN_BRIGHTEN;
    } else {
        obj->panel.style &= ~USER_INTERFACE_PANEL_SCREEN_BRIGHTEN;
    }

    status = obj->panel.control.word;
    if (status == 1) {
        if (menu->itemCount == 0) {
            Ui_SmoothCursor(&(obj)->panel, obj->panel.contentLeft.signedValue + 4, obj->panel.contentTop.signedValue + 0xA);
        }
        if (arg0->state == status) {
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
                obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                obj->result             = USER_INTERFACE_RESULT_CANCEL;
            } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
                obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                obj->result             = USER_INTERFACE_RESULT_CANCEL;
            } else if (padCheckButtons(0, PAD_BUTTON_QUERY_HELD_ANY, PAD_BUTTON_UP | PAD_BUTTON_DOWN) == 0) {
                if (arg0->spawnArg1.value == 0) {
                    if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_RIGHT) != 0) {
                        goto do_snd;
                    }
                }
                if (arg0->spawnArg1.value == status) {
                    if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_LEFT) != 0) {
                        goto do_snd;
                    }
                }
                if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_L2 | PAD_BUTTON_R2) == 0) {
                    goto children;
                }
            do_snd:
                sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
                obj->result = 0xA;
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel | Pad_MaskMenu) != 0) {
            obj->result = 0x24;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_HELD_ANY, PAD_BUTTON_UP | PAD_BUTTON_DOWN) == 0) {
            if (arg0->spawnArg1.value == 0) {
                if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_RIGHT) != 0) {
                    obj->result = 0xA;
                    goto children;
                }
            }
            if (arg0->spawnArg1.value == status) {
                if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_LEFT) != 0) {
                    obj->result = 0xA;
                    goto children;
                }
            }
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_L2 | PAD_BUTTON_R2) != 0) {
                obj->result = 0xA;
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
    TextDrawReq       req;
    s32               flags;
    InventoryItemRow* rec;
    Task*             scanOwner;
    Task*             owner;
    s32               idx;
    s32               selected;
    s32               restricted;
    s32               prompt;
    s32               chooseQty;
    s32               qty;
    s32               item;

    req.x          = arg1->panel.contentOriginX.unsignedValue + arg0->rowTextX.unsignedValue;
    req.y          = arg1->panel.contentOriginY.unsignedValue + arg0->rowTextY.unsignedValue;
    req.otIndex    = arg1->panel.otIndex.signedValue + 1;
    req.colorRgb   = arg0->colorRgb;
    req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req.alignment  = TEXT_ALIGNMENT_LEFT;
    req.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req, Gp_StrMove2);
    selected = arg0->rowInputEnabled;
    if ((selected == 1) && (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0)) {
        prompt    = -1;
        chooseQty = 0;
        idx       = arg1->owner->spawnArg1.value;
        rec       = Gp_GetScanSlot((&Gp_MoveScanSrc + (idx)), Gp_InvLists[idx].selectedItemIndex, 0);
        item      = rec->itemId;
        qty       = rec->qty;
        sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
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
            if (Gp_ItemDescs[item].flags & ITEM_FLAG_NO_DISCARD) {
                restricted = flags == 1;
            }
            if ((Gp_MoveItemKey == 0x703) && (item == 0x81) && (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage == selected)) {
                restricted = 1;
            }
            if (restricted != 0) {
                prompt = 0x1E;
            } else if ((u32)(item - 0x80) < 0x20U) {
                if ((arg1->owner->spawnArg1.value != 1) || (item != (gPlayerStatus.weapon + 0x7F))) {
                    if (prompt == -1) {
                        Gp_ClearEquipSlot(item);
                    }
                } else {
                    prompt = 7;
                }
            } else if (((u32)(item - 0x60) < 0x20U) && (arg1->owner->spawnArg1.value == 1) && (item == (gPlayerStatus.armor + 0x5F))) {
                prompt = 7;
            }
        } else {
            prompt = 6;
        }
        if (prompt >= 0) {
            Gp_SpawnItemPrompt(arg1, prompt, 0, 0);
            arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            return;
        }
        if (chooseQty == 1) {
            if (Ui_SpawnFromDesc(&D_8010D780, item, 1, 1, arg1) != NULL) {
                arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            }
        } else {
            Gp_RemoveItem((&Gp_MoveScanSrc + (arg1->owner->spawnArg1.value)), rec, qty);
            Gp_GiveItem((&Gp_MoveScanSrc + (arg1->owner->spawnArg1.value ^ 1)), item, qty);
            arg1->result = USER_INTERFACE_RESULT_CONFIRM;
        }
    }
}

void Gp_ItemActionConfirm(UiList* arg0, UiObject* arg1)
{
    TextDrawReq       req;
    s32               selected;
    s32               idx;
    InventoryItemRow* rec;
    s32               item;
    s32               flag;
    s32               flags;
    Task*             owner;
    PlayerStatus*     cfg;

    req.x          = arg1->panel.contentOriginX.unsignedValue + arg0->rowTextX.unsignedValue;
    req.y          = arg1->panel.contentOriginY.unsignedValue + arg0->rowTextY.unsignedValue;
    req.otIndex    = arg1->panel.otIndex.signedValue + 1;
    req.colorRgb   = arg0->colorRgb;
    req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req.alignment  = TEXT_ALIGNMENT_LEFT;
    req.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req, Gp_StrSwitch);

    selected = arg0->rowInputEnabled;
    if (selected == 1) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            idx  = arg1->owner->spawnArg1.value;
            rec  = Gp_GetScanSlot(&Gp_MoveScanSrc + idx, Gp_InvLists[idx].selectedItemIndex, 0);
            item = rec->itemId;
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);

            owner = arg1->owner;
            flags = owner->parent->status;
            flag  = 0;
            if (Gp_ItemDescs[item].flags & ITEM_FLAG_NO_DISCARD) {
                flag = flags == 1;
            }
            if ((Gp_MoveItemKey == 0x703) && (item == 0x81) && (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage == selected)) {
                flag = 1;
            }
            if (flag) {
                Gp_SpawnItemPrompt(arg1, 0x1E, 0, 0);
                arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            } else if (arg1->owner->spawnArg1.value == 1) {
                cfg = &gPlayerStatus;
                if ((item == cfg->weapon + 0x7F) || (item == cfg->armor + 0x5F)) {
                    Gp_SpawnItemPrompt(arg1, 7, 0, 0);
                    arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                } else {
                    arg1->result = 0x23;
                }
            } else {
                arg1->result = 0x23;
            }
        }
    }
}

/// Fills `Gp_ItemActionFns` and `arg0->field_4` / `field_5` from the selected
/// inventory row (`Gp_MoveScanSrc[spawnArg1]` / `Gp_InvLists[spawnArg1].field_10`).
static void Gp_FillItemActions(UiList* arg0, UiObject* arg1)
{
    InventoryItemRow*   rec;
    s32                 item;
    s32                 count;
    s32                 idx;
    UiListRowCallback*  table;
    Task*               owner;
    InventoryItemRange* scan;

    owner = arg1->owner;
    idx   = owner->spawnArg1.value;
    scan  = &Gp_MoveScanSrc + idx;
    rec   = Gp_GetScanSlot(scan, Gp_InvLists[idx].selectedItemIndex, 0);
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
    arg0->visibleRowCount.unsignedValue = count;
    arg0->itemCount                     = count;
}

void Gp_ItemActionListTask(Task* arg0)
{
    Task*     childTask;
    UiObject* obj;
    UiList*   menu;
    UiObject* child;
    s32       flag;
    Task*     parent;

    obj         = arg0->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    menu        = &Gp_ItemActionList;
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
    if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            if (obj->owner->status != 0) {
                obj->result = USER_INTERFACE_RESULT_CONFIRM;
            } else {
                obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                obj->result             = USER_INTERFACE_RESULT_CANCEL;
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
            obj->result = USER_INTERFACE_RESULT_CONFIRM;
        }
    }
    childTask = arg0->firstChild;
    if (childTask != NULL) {
        child = childTask->spawnArg2.pointer;
        flag  = child->result;
        switch (flag) {
            case USER_INTERFACE_RESULT_CANCEL:
                obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                obj->result             = flag;
                break;
            case USER_INTERFACE_RESULT_DISMISS:
                obj->result = USER_INTERFACE_RESULT_CONFIRM;
                break;
            case USER_INTERFACE_RESULT_CONFIRM:
                obj->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
                Ui_TeardownTree(child, child->owner);
                break;
        }
    }
}

static const char Gp_StrBullet[] = "Bullet";

void func_800BDF6C(Task* task)
{
    u8                      buf[0x20];
    s32                     color;
    s32                     width;
    s32                     widthM2;
    s32                     half;
    InventoryItemRange*     consumeScan;
    LINE_F2*                line;
    UiObject*               obj;
    s16                     panelY;
    s16                     coord;
    s32                     srcLimit;
    s32                     remaining;
    s32                     dstLimit;
    s32                     moveAllLimit;
    s32                     sourceQty;
    s32                     equippedWidth;
    s32                     textY;
    s32                     splitWidth;
    s32                     caretX;
    s32                     status;
    s32                     caretY;
    s32                     usableWidth;
    s32                     srcTotal;
    s32                     dstTotal;
    s32                     destAfterStep;
    s32                     negWidth;
    s32                     halfWidth;
    s32                     qty;
    s32                     totalQty;
    s32                     equipped;
    s32                     srcAfterMove;
    s32                     destAfterClamp;
    s32                     sourceToMove;
    s32                     combinedQty;
    s32                     destQty;
    s32                     repeatStep;
    s32                     transferQty;
    s32                     stepToSource;
    u8                      message;
    s16                     result;
    PadState*               pad;
    InventoryItemRange*     sourceScan;
    InventoryItemRange*     dstScan;
    _ItemMenuAmmoSplitWork* split;

    obj         = task->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    width       = (obj->panel.contentRight.signedValue - obj->panel.contentLeft.signedValue) - 0x50;
    Ui_DrawText(&(obj)->panel, (char*)Gp_StrBullet);
    if (task->state == 0) {
        split = memCalloc(sizeof(*split), 0);
        if (split == NULL) {
            obj->result = USER_INTERFACE_RESULT_DISMISS;
            return;
        }
        task->work                 = split;
        srcTotal                   = Gp_ScanStackQty(&Gp_MoveScanSrc, task->spawnArg1.value);
        split->containerQty        = srcTotal;
        split->containerInitialQty = srcTotal;
        dstTotal                   = Gp_ScanStackQty(&Gp_MoveScanSrc + 1, task->spawnArg1.value);
        split->carriedQty          = dstTotal;
        split->carriedInitialQty   = dstTotal;
        split->loadedQty           = Gp_CountEquippedRelated(&Gp_MoveScanSrc + 1, task->spawnArg1.value);
        Ui_SetHolderParam(Gp_StrSetAmmoHelp, 0, 0);
        split->stackLimit = Gp_StackLimits[task->spawnArg1.value - 0xA0].maxHeld;
        task->state       = task->state + 1;
    }
    split = task->work;
    Gp_DrawItemLabel(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0xF, task->spawnArg1.value, 0x606060, 0);
    task->status = 0;
    totalQty     = split->containerQty + split->carriedQty;
    color        = 0x606060;
    if (width < totalQty) {
        repeatStep = totalQty / width;
    } else {
        repeatStep = 1;
    }
    pad = gPadStates;
    if (pad->directionRepeatTicks != 0) {
        pad->directionRepeatTicks += gDisplayState.frameTicks * 2;
    }
    status = obj->panel.control.word;
    if (status == 1) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_HELD_ANY, PAD_BUTTON_UP | PAD_BUTTON_DOWN) == 0) {
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_LEFT) != 0) {
                qty      = split->carriedQty;
                equipped = split->loadedQty;
                if (equipped < qty) {
                    stepToSource = 1;
                    if (pad->directionRepeatTicks >= 0x14U) {
                        stepToSource = repeatStep;
                    }
                    split->containerQty += stepToSource;
                    split->carriedQty   -= stepToSource;
                    if (split->carriedQty < split->loadedQty) {
                        split->containerQty += split->carriedQty - split->loadedQty;
                        split->carriedQty    = split->loadedQty;
                    }
                    srcAfterMove = split->containerQty;
                    srcLimit     = split->stackLimit;
                    if (srcLimit < srcAfterMove) {
                        split->containerQty = srcLimit;
                        split->carriedQty   = split->carriedQty + (srcAfterMove - srcLimit);
                        goto step_at_capacity;
                    }
                } else if (equipped > 0) {
                    task->status = (u8)status;
                }
            } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_RIGHT) != 0) {
                {
                    s32 step;

                    step = 1;
                    if (pad->directionRepeatTicks >= 0x14U) {
                        step = repeatStep;
                    }
                    split->containerQty = split->containerQty - step;
                    destAfterStep       = split->carriedQty + step;
                    split->carriedQty   = destAfterStep;
                    remaining           = split->containerQty;
                    if (remaining < 0) {
                        split->carriedQty   = destAfterStep + remaining;
                        split->containerQty = 0;
                    }
                }
                destAfterClamp = split->carriedQty;
                dstLimit       = split->stackLimit;
                if (dstLimit < destAfterClamp) {
                    split->carriedQty   = dstLimit;
                    split->containerQty = split->containerQty + (destAfterClamp - dstLimit);
                step_at_capacity:
                    task->status = 2U;
                }
            }
        }
        if (padCheckButtons(0, PAD_BUTTON_QUERY_HELD_ANY, PAD_BUTTON_RIGHT | PAD_BUTTON_LEFT) == 0) {
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_L2 | PAD_BUTTON_L1 | PAD_BUTTON_UP) != 0) {
                if (split->carriedQty > split->loadedQty) {
                    s32 total;

                    total  = split->containerQty + split->carriedQty;
                    total -= split->loadedQty;
                    if (total < split->stackLimit) {
                        split->carriedQty   = split->loadedQty;
                        split->containerQty = total;
                    } else {
                        split->containerQty = split->stackLimit;
                        split->carriedQty   = split->loadedQty + (total - split->stackLimit);
                        task->status        = 2;
                    }
                } else if (split->loadedQty > 0) {
                    task->status = 1;
                }
            } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_R2 | PAD_BUTTON_R1 | PAD_BUTTON_DOWN) != 0) {
                sourceToMove = split->containerQty;
                if (sourceToMove > 0) {
                    moveAllLimit = split->stackLimit;
                    combinedQty  = sourceToMove + split->carriedQty;
                    if (combinedQty < moveAllLimit) {
                        split->carriedQty   = combinedQty;
                        split->containerQty = 0;
                    } else {
                        split->carriedQty   = moveAllLimit;
                        split->containerQty = combinedQty - split->stackLimit;
                        task->status        = 2U;
                    }
                }
            }
        }
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            transferQty = split->containerQty - split->containerInitialQty;
            if (transferQty > 0) {
                sourceScan = &Gp_MoveScanSrc;
                Gp_GiveItem(sourceScan, task->spawnArg1.value, transferQty);
                consumeScan = sourceScan + 1;
                goto consume_transfer;
            }
            result = USER_INTERFACE_RESULT_DISMISS;
            if (transferQty < 0) {
                transferQty = -transferQty;
                dstScan     = &Gp_MoveScanDst;
                Gp_GiveItem(dstScan, task->spawnArg1.value, transferQty);
                consumeScan = dstScan - 1;
            consume_transfer:
                Gp_ConsumeScanQty(consumeScan, task->spawnArg1.value, transferQty);
                result = USER_INTERFACE_RESULT_DISMISS;
            }
            goto set_result;
        }
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
            result                  = USER_INTERFACE_RESULT_CANCEL;
            obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            goto set_result;
        }
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
            result = USER_INTERFACE_RESULT_DISMISS;
        set_result:
            obj->result = result;
        }
    }
    destQty = split->carriedQty;
    if ((destQty == split->loadedQty) && (destQty > 0)) {
        color = 0x37A78;
    }
    sourceQty   = split->containerQty;
    usableWidth = width - 2;
    widthM2     = usableWidth;
    panelY      = obj->panel.contentTop.signedValue;
    splitWidth  = ((s32)(sourceQty * usableWidth) / (s32)(sourceQty + split->carriedQty)) + 1;
    textY       = panelY + 0x20;
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 0x20, textY, Text_ItoaUnsigned(buf, (u32)sourceQty), 0x606060, TEXT_DRAW_OUTLINED,
                   TEXT_ALIGNMENT_RIGHT);
    textDrawUiLine(obj, obj->panel.contentRight.signedValue - 6, textY, Text_ItoaUnsigned(buf, (u32)split->carriedQty), color, TEXT_DRAW_OUTLINED,
                   TEXT_ALIGNMENT_RIGHT);
    caretY    = panelY + 0x16;
    negWidth  = -width;
    halfWidth = (s32)(negWidth + ((u32)negWidth >> 0x1F)) >> 1;
    caretX    = halfWidth + splitWidth;
    half      = halfWidth;
    Ui_DrawFlatCaret(&(obj)->panel, caretX, caretY, 0x606060, 1);
    Ui_DrawFlatCaret(&(obj)->panel, caretX, panelY + 0x1E, 0x606060, 0);
    line                              = gGpuPrimCursor;
    gGpuPrimCursor                    = line + 1;
    GPU_PRIMITIVE_COLOR_WORD(line, 0) = GPU_PACK_COLOR_WORD(0x60, 0x60, 0x60, 0);
    coord                             = obj->panel.contentOriginX.unsignedValue + caretX;
    line->x1                          = coord;
    line->x0                          = coord;
    line->y0                          = (obj->panel.contentOriginY.unsignedValue + textY) - 0xA;
    coord                             = (obj->panel.contentOriginY.unsignedValue + textY) - 2;
    setlen(line, 3);
    setcode(line, 0x40);
    line->y1 = coord;
    addPrim(gGpuCurrentOt + obj->panel.otIndex.signedValue + 1, line);
    qty = split->loadedQty;
    if (qty > 0) {
        equippedWidth = ((s32)(qty * widthM2) / (s32)(split->containerQty + split->carriedQty)) + 2;
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
static const _ItemMenuPromptTexts Gp_ItemPromptTexts = { Gp_StrAll, Gp_StrSelect, Gp_StrDiscard, Gp_StrEnd };
/// Fullscreen-fade vector template used by `Gp_FadeTileTask` / `Gp_ItemPickupTilt`.
static const VECTOR D_80093DB0 = { 0, -100, 0, 0 };

/// For each carried weapon, empties its primary and secondary ammunition
/// counts when the carried items no longer include any of that item.
static inline void _gpDropOrphanedWeaponLoads(void)
{
    InventoryItemRange*  scan;
    InventoryItemRow*    rec;
    EquipmentWeaponLoad* slot;
    s32                  i;
    s32                  loadedItemId;

    scan = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    rec  = Gp_GetItemTable(scan);
    i    = 0;
    rec  = &rec[scan->firstRow];
    if (scan->rowCount != 0) {
        do {
            if ((u8)(rec->itemId + 0x80) < 0x20) {
                slot         = Gp_GetItemSlot(rec->itemId);
                loadedItemId = slot->primaryItemId;
                // Battery 0xB9 is a built-in primary supply and has no carried stack.
                if ((loadedItemId != INVENTORY_ITEM_NONE) && (loadedItemId != 0xB9)) {
                    if (Gp_SumScanQty(scan, loadedItemId) == 0) {
                        slot->primaryQty = 0;
                    }
                }
                loadedItemId = slot->secondaryItemId;
                // Built-in secondary supplies (Battery 0xB5/0xBB/0xBE, Fuel 0xBD) have no carried stack.
                if ((loadedItemId != INVENTORY_ITEM_NONE) && (loadedItemId != EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE) && (loadedItemId != 0xB5) && (loadedItemId != 0xBB) &&
                    (loadedItemId != 0xBD) && (loadedItemId != 0xBE)) {
                    if (Gp_SumScanQty(scan, loadedItemId) == 0) {
                        slot->secondaryQty = 0;
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
    _ItemMenuPromptTexts labels;
    s32                  row;

    labels = Gp_ItemPromptTexts;
    if (arg0->currentItemIndex == ITEM_MENU_PROMPT_ALL) {
        if (arg1->owner->spawnArg1.value == 0) {
            arg0->colorRgb = Ui_LookupTable(arg1, 2);
            if (arg0->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
                arg0->navigationStep  = USER_INTERFACE_LIST_STEP_NEXT;
                arg0->actionResult    = USER_INTERFACE_LIST_ACTION_SKIP_ROW;
                arg0->rowInputEnabled = USER_INTERFACE_LIST_ROW_INACTIVE;
            }
        }
    }
    row = arg0->currentItemIndex;
    textDrawUiLine(arg1, arg0->rowTextX.signedValue, arg0->rowTextY.signedValue, labels.label[row], arg0->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);

    if (arg0->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (arg0->currentItemIndex == ITEM_MENU_PROMPT_DISCARD) {
            if (arg0->actionResult == ITEM_MENU_LIST_ACTION_CANCEL_HANDLED) {
                arg0->actionResult = USER_INTERFACE_RESULT_NONE;
                return;
            }
        }
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            switch (arg0->currentItemIndex) {
                case ITEM_MENU_PROMPT_ALL:
                    sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                    arg1->result = 0x26;
                    break;
                case ITEM_MENU_PROMPT_SELECT:
                    sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                    arg1->result = USER_INTERFACE_RESULT_CONFIRM;
                    break;
                case ITEM_MENU_PROMPT_DISCARD:
                    sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
                    _gpDropOrphanedWeaponLoads();
                    arg1->result = 0x27;
                    break;
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
            if (arg0->selectedItemIndex == ITEM_MENU_PROMPT_DISCARD) {
                _gpDropOrphanedWeaponLoads();
                arg1->result = 0x27;
            } else {
                arg0->actionResult      = ITEM_MENU_LIST_ACTION_CANCEL_HANDLED;
                arg0->selectedItemIndex = ITEM_MENU_PROMPT_DISCARD;
            }
        }
    }
}

/// Task callback. `extra` is a `TmdObject`; `spawnArg2` is the placed object's
/// `Enemy`, whose `placeKey` low byte names its 2-bit flag and whose `workType`
/// is the `AreaObjectPlace.kind` it was spawned from.
/// Tilts `coords[2]` (a `GfxCoord`) while playing a location-specific
/// type-6 sound, then signals `extraState` (`CapActionRequest.done = 1`) when
/// the motion returns to 0.
void Gp_ItemPickupTilt(Task* arg0)
{
    GameSession*      session;
    TmdObject*        extra;
    Enemy*            enemy;
    GfxCoord*         coord;
    GfxCoord*         rot;
    VECTOR            vec;
    VECTOR            vec2;
    MATRIX*           mem;
    CapActionRequest* request;
    u32               stageAreaKey;
    s32               room;
    s32               check;
    u16               item;

    extra        = arg0->extra.tmd;
    enemy        = arg0->spawnArg2.pointer;
    session      = gGameSession;
    stageAreaKey = GAME_LOCATION_WORD(session->location.loc) & GAME_LOCATION_STAGE_AREA_VIEW_MASK;
    item         = enemy->workType;
    coord        = extra->coords;
    rot          = coord + 2;
    room         = *&session->location.loc.view;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_HIDDEN) {
        extra->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        extra->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    stageAreaKey &= GAME_LOCATION_STAGE_AREA_MASK;
    if (stageAreaKey == GAME_LOCATION_KEY(4, 16, 0, 0)) {
        if ((u32)(room - 8) >= 2) {
            extra->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        }
    } else if (stageAreaKey == GAME_LOCATION_KEY(4, 31, 0, 0)) {
        check = 3;
        goto compare_room;
    } else if (stageAreaKey == GAME_LOCATION_KEY(4, 20, 0, 0)) {
        check = 0x11;
    compare_room:
        if (room != check) {
            extra->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        }
    }
    if (arg0->state == 0) {
        mem = memCalloc(0x40, 0);
        if (mem != NULL) {
            vec             = D_80093DB0;
            extra->lightMtx = mem;
            extra->colorMtx = mem + 1;
            actorRenderComposeCoord(arg0->extra.tmd->coords);
            worldCoordSetModelLighting(extra, &vec, 0, 3);
            arg0->work = mem;
        }
        arg0->msgTable = D_8010D828;
        arg0->status   = 0;
        extra->flags   = 0;
        arg0->state++;
    } else if (arg0->state == 1) {
        if (Gp_GetCurBit2Flag((u8)enemy->placeKey) != 2) {
            if (arg0->status != 0) {
                arg0->killCountdown = 0;
                switch (stageAreaKey) {
                    case GAME_LOCATION_KEY(1, 6, 0, 0): {
                        s32 temp;
                        temp = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                        sndEvtRequestScriptStart(SOUND_ACROPOLIS_SECURITY_ROOM_ITEM_LID_OPEN, temp,
                                                 (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
                        break;
                    }
                    case GAME_LOCATION_KEY(1, 12, 0, 0): {
                        s32 temp;
                        temp = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                        sndEvtRequestScriptStart(SOUND_ACROPOLIS_SANCTUARY_ITEM_LID_OPEN, temp,
                                                 (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
                    }
                    case GAME_LOCATION_KEY(2, 27, 0, 0): {
                        s32 temp;
                        temp = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                        sndEvtRequestScriptStart(SOUND_TRAILER_COACH_ITEM_LID_OPEN, temp,
                                                 (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
                        break;
                    }
                    case GAME_LOCATION_KEY(3, 27, 0, 0): {
                        s32 temp;
                        temp = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                        sndEvtRequestScriptStart(SOUND_NIGHT_TRAILER_COACH_ITEM_LID_OPEN, temp,
                                                 (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
                        break;
                    }
                    case GAME_LOCATION_KEY(4, 16, 0, 0): {
                        s32 temp;
                        temp = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                        sndEvtRequestScriptStart(SOUND_SHELTER_B1_STERILIZATION_ITEM_LID_OPEN, temp,
                                                 (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
                        break;
                    }
                    case GAME_LOCATION_KEY(4, 31, 0, 0): {
                        s32 temp;
                        temp = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                        sndEvtRequestScriptStart(SOUND_SHELTER_B2_LAB_ITEM_LID_OPEN, temp,
                                                 (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
                        break;
                    }
                    case GAME_LOCATION_KEY(4, 39, 0, 0): {
                        s32 temp;
                        temp = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                        sndEvtRequestScriptStart(SOUND_SHELTER_B3_DUMPING_HOLE_ITEM_LID_OPEN, temp,
                                                 (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
                        break;
                    }
                }
                arg0->state++;
            }
        }
    } else if (arg0->state == 2) {
        arg0->killCountdown++;
        gfxRotMatrixX(&rot->coord, arg0->killCountdown << 5, GRAPHICS_ROTATION_REPLACE);
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
            switch (stageAreaKey) {
                case GAME_LOCATION_KEY(1, 6, 0, 0): {
                    s32 temp;
                    temp = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                    sndEvtRequestScriptStart(SOUND_ACROPOLIS_SECURITY_ROOM_ITEM_LID_CLOSE, temp,
                                             (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
                    break;
                }
                case GAME_LOCATION_KEY(1, 12, 0, 0): {
                    s32 temp;
                    temp = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                    sndEvtRequestScriptStart(SOUND_ACROPOLIS_SANCTUARY_ITEM_LID_CLOSE, temp,
                                             (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
                    break;
                }
                case GAME_LOCATION_KEY(2, 27, 0, 0):
                    break;
                case GAME_LOCATION_KEY(3, 27, 0, 0): {
                    s32 temp;
                    temp = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                    sndEvtRequestScriptStart(SOUND_NIGHT_TRAILER_COACH_ITEM_LID_CLOSE, temp,
                                             (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
                    break;
                }
                case GAME_LOCATION_KEY(4, 16, 0, 0): {
                    s32 temp;
                    temp = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                    sndEvtRequestScriptStart(SOUND_SHELTER_B1_STERILIZATION_ITEM_LID_CLOSE, temp,
                                             (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
                    break;
                }
                case GAME_LOCATION_KEY(4, 31, 0, 0): {
                    s32 temp;
                    temp = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                    sndEvtRequestScriptStart(SOUND_SHELTER_B2_LAB_ITEM_LID_CLOSE, temp,
                                             (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
                    break;
                }
                case GAME_LOCATION_KEY(4, 39, 0, 0): {
                    s32 temp;
                    temp = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                    sndEvtRequestScriptStart(SOUND_SHELTER_B3_DUMPING_HOLE_ITEM_LID_CLOSE, temp,
                                             (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
                    break;
                }
            }
            arg0->state++;
        }
        arg0->killCountdown -= 4;
        if (arg0->killCountdown <= 0) {
            arg0->killCountdown = 0;
        }
        gfxRotMatrixX(&rot->coord, arg0->killCountdown << 5, GRAPHICS_ROTATION_REPLACE);
        rot->composeStamp = GRAPHICS_COORD_DIRTY;
        if (arg0->killCountdown == 0) {
            arg0->status = 0;
            request      = arg0->extraState.pointer;
            if (request != NULL) {
                request->done          = 1;
                arg0->extraState.value = 0;
            }
            arg0->state = 1;
        }
    }
    vec2 = D_80093DB0;
    actorRenderComposeCoord(arg0->extra.tmd->coords);
    worldCoordSetModelLighting(extra, &vec2, 0, 3);
}

static void Gp_ForEachUiChild(UiObject* arg0, UiObjectTaskFunc arg1)
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
    if (Gp_ItemDescs[arg0].flags & ITEM_FLAG_NO_DISCARD) {
        ret = arg1 == 1;
    }
    if ((Gp_MoveItemKey == 0x703) && (arg0 == 0x81) && (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage == GAME_STAGE_ACROPOLIS)) {
        ret = 1;
    }
    return ret;
}

/// Child closer for the `Gp_InvLists` inventory panes. Cancel tears down and
/// either restores parent status or sets the parent `result` to cancel when the
/// parent owner has no flags; confirm / `0x23` / `38` / `39` copy those codes
/// onto the parent (confirm also restores status).
static void Gp_CloseItemPane(UiObject* arg0, Task* arg1)
{
    UiObject* parent;

    parent = arg1->parent->spawnArg2.pointer;
    switch (arg0->result) {
        case USER_INTERFACE_RESULT_CANCEL:
            if (parent->owner->status) {
                parent->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
                Ui_TeardownTree(arg0, arg0->owner);
            } else {
                Ui_TeardownTree(arg0, arg0->owner);
                parent->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                parent->result             = USER_INTERFACE_RESULT_CANCEL;
            }
            break;
        case USER_INTERFACE_RESULT_CONFIRM:
            parent->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
            Ui_TeardownTree(arg0, arg0->owner);
            break;
        case 0x23:
            Ui_TeardownTree(arg0, arg0->owner);
            parent->result = 0x23;
            break;
        case 38:
        case 39:
            parent->result = arg0->result;
            break;
    }
}

void Gp_ItemMenuListTask(Task* arg0)
{
    UiObject* obj;
    UiList*   menu;

    obj         = arg0->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    menu        = &Gp_ItemMenuList;
    if (arg0->state == 0) {
        Ui_LayoutListPanel(menu, &(obj)->panel);
        if (arg0->spawnArg1.value == 0) {
            menu->selectedItemIndex = ITEM_MENU_PROMPT_SELECT;
        } else {
            menu->selectedItemIndex = ITEM_MENU_PROMPT_ALL;
        }
        menu->flags  = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        arg0->state += 1;
    }
    Ui_UpdateListNoAnim(menu, obj);
}

void Gp_HolderPromptTask(Task* arg0)
{
    UiObject* obj;
    u8*       val;
    s32       color;
    s32       one;
    const u8* text;

    obj         = arg0->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    if (arg0->state == 0) {
        Wip_UiHolder = obj;
        arg0->state += 1;
    }
    val = arg0->spawnArg1.pointer;
    if (val != 0) {
        color = 0x606060;
        one   = 1;
        textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0xF, val, color, one, TEXT_ALIGNMENT_LEFT);
        text = textSkipLines(val, one);
        textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0x1E, text, color, one, TEXT_ALIGNMENT_LEFT);
    }
}

s32 Gp_BindItemObj2(Task* arg0, s32 arg1, CapActionRequest* request, s32 arg3)
{
    s32    flag;
    Enemy* enemy;

    enemy                    = arg0->spawnArg2.pointer;
    flag                     = 1;
    arg0->status             = flag;
    arg0->extraState.pointer = request;
    if (Gp_GetCurBit2Flag((u8)enemy->placeKey) == 2) {
        request->done = flag;
    }
    return 0;
}

void Gp_PublishItemObj(Task* arg0)
{
    Enemy* enemy = arg0->spawnArg2.pointer;
    s32    count;

    Gp_PubItemId  = (u8)enemy->placeKey;
    Gp_PubItemLoc = enemy->workType;
    if (enemy->workType < 0xA0) {
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
        count           = Gp_StackLimits[Gp_PubItemLoc - 0xA0].packQty;
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

    addPrim(&gGpuCurrentOt[otIdx], tile);
    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE1000640;
    addPrim(&gGpuCurrentOt[otIdx], dr);

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
