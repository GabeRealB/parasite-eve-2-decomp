#include "gameplay/item_menu.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "gameplay/actor_render.h"
#include "cap.h"
#include "gameplay/display.h"
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
#include "main/fs.h"
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

/// Commands a child dialog forwards through its inventory pane to the move screen.
enum {
    ITEM_MENU_RESULT_CHANGE_PANE          = 0xA,
    ITEM_MENU_RESULT_CANCEL_SWAP          = 0x24,
    ITEM_MENU_RESULT_CONFIRM_SWAP_PARTNER = 0x25,
    ITEM_MENU_RESULT_BEGIN_SWAP           = 0x23,
    ITEM_MENU_RESULT_MOVE_ALL             = 0x26,
    ITEM_MENU_RESULT_DISCARD_AND_EXIT     = 0x27
};

/// Transfer-pane indices, interaction states and the battle-field spawn flag.
enum {
    ITEM_MENU_PANE_CONTAINER             = 0,
    ITEM_MENU_PANE_CARRIED               = 1,
    ITEM_MENU_PANE_BROWSING              = 1,
    ITEM_MENU_PANE_CHOOSING_SWAP_PARTNER = 2,
    ITEM_MENU_PANE_BATTLE_FIELD_FLAG     = 0x100,
    ITEM_MENU_PANE_INDEX_MASK            = 0xFF,
    ITEM_MENU_PANE_MODE_ITEM_BOX         = 0,
    ITEM_MENU_PANE_MODE_BATTLE_FIELD     = 1,
    ITEM_MENU_PANE_VISIBLE_ROWS          = 10
};

/// Catalogue boundaries and the Acropolis container's M93R transfer restriction.
enum {
    ITEM_MENU_ITEM_RECOVERY_FIRST            = 1,
    ITEM_MENU_ITEM_RECOVERY_COUNT            = 3,
    ITEM_MENU_ITEM_COLA                      = 5,
    ITEM_MENU_ITEM_MP_BOOST1                 = 6,
    ITEM_MENU_ITEM_MP_BOOST2                 = 7,
    ITEM_MENU_ITEM_PROTEIN_CAPSULE           = 0x3C,
    ITEM_MENU_ITEM_RINGER                    = 0x3D,
    ITEM_MENU_ARMOR_ITEM_FIRST               = 0x60,
    ITEM_MENU_EQUIPMENT_ITEM_COUNT           = 0x20,
    ITEM_MENU_M93R_ITEM_ID                   = 0x81,
    ITEM_MENU_M93R_RESTRICTED_CONTAINER_KIND = 0x703,
    ITEM_MENU_DUPLICATE_ARMOR_ITEM_ID        = 0xD,
    ITEM_MENU_DUPLICATE_WEAPON_ITEM_ID       = 0x3D,
    ITEM_MENU_NOTICE_NO_TRANSFER_SPACE       = 6,
    ITEM_MENU_ACTION_PANEL_BOTTOM            = 100,
    ITEM_MENU_NORMAL_COLOR_RGB               = 0x606060,
    ITEM_MENU_LOADED_AMMO_COLOR_RGB          = 0x037A78
};

/// Quantity-panel notice states and timing in callback ticks / repeat ticks.
enum {
    ITEM_MENU_AMMO_NOTICE_NONE             = 0,
    ITEM_MENU_AMMO_NOTICE_LOADED           = 1,
    ITEM_MENU_AMMO_NOTICE_CAPACITY         = 2,
    ITEM_MENU_AMMO_NOTICE_TICKS            = 188,
    ITEM_MENU_AMMO_ACCELERATE_REPEAT_TICKS = 20,
    ITEM_MENU_AMMO_BAR_BACKGROUND_RGB      = 0x102010
};

/// Supply item ids preserved even though no carried stack backs their charge.
enum {
    EQUIPMENT_SUPPLY_HYPERVELOCITY_BATTERY = 0xB9,
    EQUIPMENT_SUPPLY_P229_MP5_BATTERY      = 0xB5,
    EQUIPMENT_SUPPLY_HAMMER_BATTERY        = 0xBB,
    EQUIPMENT_SUPPLY_PYKE_FUEL             = 0xBD,
    EQUIPMENT_SUPPLY_JAVELIN_BATTERY       = 0xBE
};

/// Fixed extent and counter-to-grey conversion of the display transition overlay.
enum {
    FADE_DISPLAY_WIDTH_PIXELS    = 320,
    FADE_DISPLAY_HEIGHT_PIXELS   = 240,
    FADE_DISPLAY_STEPS           = 8,
    FADE_DISPLAY_INTENSITY_SHIFT = 5,
    FADE_DISPLAY_MIN_GREY        = (1 << FADE_DISPLAY_INTENSITY_SHIFT) - 1,
    FADE_DISPLAY_MAX_GREY        = 255,
    FADE_DISPLAY_WORLD_OT        = 0,
    FADE_DISPLAY_CLEAR_IMAGE_OT  = 59,
    FADE_DISPLAY_TASK_OT         = 63
};

/// GPU draw mode with dithering, drawing to the display area and subtractive blending.
enum { FADE_DISPLAY_DRAW_MODE = 0xE1000600 | (GPU_BLEND_SUBTRACT << 5) };

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

/// UiList used by `_itemMenuTransferActionsTask`.
extern UiList Gp_ItemActionList;

/// UiList used by `_itemMenuTransferExitMenuTask`. `selectedItemIndex` is Select when `spawnArg1` is 0.
extern UiList Gp_ItemMenuList;

extern UiListRowCallback D_8010D6B0[1];

extern TaskMessageEntry D_8010D828[2];

/* Kept next to Gp_ItemMoveChild's jump table so the overlay .rodata stays packed;
   Gp_StrBullet follows before _itemMenuAmmoSplitTask. */
static const char Gp_StrBattleField[];

static const char Gp_StrItemBox[];

static const char Gp_StrPlayerItem[];

static const char Gp_StrBullet[];

/* After Gp_StrBullet from _itemMenuAmmoSplitTask so the overlay .rodata stays packed. */
static const _ItemMenuPromptTexts Gp_ItemPromptTexts;

/// Vector template used by `Gp_ItemPickupTilt`.
static const VECTOR D_80093DB0;

/// Task callback for the item-move UI. `spawnArg2` is the `UiObject`.
/// First run copies `Gp_ScanPtrs[Gp_PubItemLoc]` / `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems`
/// into `Gp_MoveScanSrc` / `Gp_MoveScanDst`, spawns the `D_8010D6F4` pair
/// (plus `[9]` when `spawnArg1 == 1`), then walks children through
/// `Gp_ItemMoveChild`. Always writes `resultValue = 0x34`.
void Gp_ItemMoveTask(Task* arg0);

static void _itemMenuTransferPaneTask(Task* task);

static void _itemMenuTransferActionsTask(Task* task);

static void _itemMenuAmmoSplitTask(Task* task);

static void _itemMenuTransferExitRow(UiList* list, UiObject* object);

static void _itemMenuTransferExitMenuTask(Task* task);

static void _itemMenuHolderPromptTask(Task* task);

static s32 _itemPickupHandleLidActionRequest(Task* task, s32 messageId, CapActionRequest* request, s32 unused);

/// Per-child item-move handler. Walked by `Gp_ItemMoveTask` over
/// `obj->owner`'s children as `Gp_ItemMoveChild(child->spawnArg2.pointer, child)`.
static void Gp_ItemMoveChild(UiObject* arg0, Task* arg1);

static inline InventoryItemRange* _itemMenuGetPaneRange(const Task* task);

static void _itemMenuFillTransferActions(UiList* list, UiObject* object);

static inline void _equipmentClearOrphanedWeaponLoads(void);

static void _uiVisitChildObjects(const UiObject* object, UiObjectTaskFunc visitChild);

static s32 _itemMenuIsTransferRestricted(s32 itemId, s32 battleFieldMode);

static void _itemMenuHandlePaneChildResult(UiObject* child, Task* childTask);

UiList            Gp_ItemActionList = { Gp_ItemActionFns, 3, { 3 }, 1, 10, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };
UiListRowCallback D_8010D6B0[1]     = { _itemMenuTransferExitRow };
UiList            Gp_ItemMenuList   = { D_8010D6B0, 3, { 3 }, 0, 15, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 };
UiObjectDesc      D_8010D6D8        = { (s32)USER_INTERFACE_PANEL_NO_FRAME, { -100, -30, 200, 60 }, 36, 0, TASK_BODY_NONE, 192, Gp_ItemMoveTask, 0 };
UiObjectDesc      D_8010D6F4[11]    = {
    { 0x80000 | USER_INTERFACE_PANEL_TITLE_STYLE, { -144, -104, 144, 160 }, 56, 0, TASK_BODY_NONE, 192, _itemMenuTransferPaneTask, 0 },
    { 0x80000 | USER_INTERFACE_PANEL_TITLE_STYLE, { 0, -104, 144, 160 }, 52, 0, TASK_BODY_NONE, 192, _itemMenuTransferPaneTask, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { 0, 0, 70, 46 }, 16, 0, TASK_BODY_NONE, 192, _itemMenuTransferActionsTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -100, -30, 200, 50 }, 8, 0, TASK_BODY_NONE, 192, _itemMenuAmmoSplitTask, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { 0, 0, 0, 0 }, 0, 0, TASK_BODY_NONE, 0, NULL, 0 },
    { 0, { -58, -30, 116, 60 }, 16, 0, TASK_BODY_NONE, 192, _itemMenuTransferExitMenuTask, 0 },
    { 0, { -144, 64, 288, 40 }, 60, 0, TASK_BODY_NONE, 192, _itemMenuHolderPromptTask, 0 },
};
TaskMessageEntry D_8010D828[2] = { { CAP_ACTION_MESSAGE_REQUEST, _itemPickupHandleLidActionRequest }, { TASK_MESSAGE_TABLE_END, NULL } };

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
            tbl     = inventoryGetRangeTable(scanSrc);
            base    = scanSrc->firstRow;
            i       = 0;
            if (scanSrc->rowCount != 0) {
                do {
                    if (tbl[base].itemId != INVENTORY_ITEM_NONE) {
                        inventoryGiveItem(&Gp_MoveScanDst, tbl[base].itemId, tbl[base].qty);
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
                flag = inventoryCountOccupiedRows(&Gp_MoveScanSrc) > 0;
            }
            work->panes[work->focusedPane]->owner->state     = 1;
            work->panes[work->focusedPane ^ 1]->owner->state = 1;
            if ((arg0->result != 0x26) && flag) {
                val = Gp_CanMoveItems();
                sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
                work->focusedPane = 0;
                uiSpawnObject(&D_8010D6F4[9], val, 1, 1, work->panes[0]);
                break;
            }
            /* fallthrough */
        case 0x27:
            gGameSession->uiOpen = 0;
            obj->result          = USER_INTERFACE_RESULT_CANCEL;
            break;
        case USER_INTERFACE_RESULT_CONFIRM:
            uiStartTreeClosing(arg0, arg1);
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
                recDst = inventoryGetRow(dst, rowDst, 0);
                qtyDst = recDst->qty;
                idDst  = recDst->itemId;
                inventoryRemoveItemRow(dst, recDst, qtyDst);
                src    = dst - 1;
                recSrc = inventoryGetRow(src, rowSrc, 0);
                qtySrc = recSrc->qty;
                idSrc  = recSrc->itemId;
                inventoryRemoveItemRow(src, recSrc, qtySrc);
                inventoryPlaceItemAtRow(dst, rowDst, idSrc, qtySrc);
                inventoryPlaceItemAtRow(src, rowSrc, idDst, qtyDst);
                if ((u8)(recDst->itemId + 0x80) < 0x20) {
                    equipmentClearRemovableLoads(recDst->itemId);
                }
            } else {
                rowA = work->swapRow;
                rowB = work->swapPartnerRow;
                if (rowA != rowB) {
                    scan            = &Gp_MoveScanSrc + work->swapPane;
                    recA            = inventoryGetRow(scan, rowA, 0);
                    qtyA            = recA->qty;
                    idA             = recA->itemId;
                    attachmentSlotA = recA->attachSlot;
                    inventoryRemoveItemRow(scan, recA, qtyA);
                    recB            = inventoryGetRow(scan, rowB, 0);
                    qtyB            = recB->qty;
                    idB             = recB->itemId;
                    attachmentSlotB = recB->attachSlot;
                    inventoryRemoveItemRow(scan, recB, qtyB);
                    inventoryPlaceItemAtRow(scan, rowA, idB, qtyB)->attachSlot = attachmentSlotB;
                    inventoryPlaceItemAtRow(scan, rowB, idA, qtyA)->attachSlot = attachmentSlotA;
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
   Gp_StrBullet follows before _itemMenuAmmoSplitTask. */
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
        itemMenuClearPreviewItems();
        work = memCalloc(sizeof(*work), 0);
        i    = 0;
        if (work == NULL) {
            obj->result             = USER_INTERFACE_RESULT_CANCEL;
            obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            obj->resultValue        = 0x34;
            return;
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
        inventorySortItems(&Gp_MoveScanSrc, 0);
        if (arg0->spawnArg1.value == 1) {
            val               = Gp_CanMoveItems();
            work->focusedPane = 0;
            work->panes[0]    = uiSpawnObject(D_8010D6F4, 0x100, 0, 1, obj);
            work->panes[1]    = uiSpawnObject(D_8010D6F4 + 1, 0x101, 0, 1, obj);
            uiSpawnObject(D_8010D6F4 + 9, val, 1, 1, work->panes[0]);
        } else {
            work->focusedPane       = 0;
            work->panes[0]          = uiSpawnObject(D_8010D6F4, 0, 1, 1, obj);
            work->panes[1]          = uiSpawnObject(D_8010D6F4 + 1, 1, 0, 1, obj);
            obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        }
        uiSpawnObject(&D_8010D6F4[10], 0, 0, 1, obj);
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
    obj->resultValue = 0x34;
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

    rec  = inventoryGetRow(&Gp_MoveScanSrc + arg1->owner->spawnArg1.value, arg0->currentItemIndex, 0);
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
            itemMenuSetPreviewItem(item, CD_COMMAND_DISPLAY_LOAD_MENU);
            itemMenuSetItemDescriptionPrompt(item);
        }
    }
    if (arg1->owner->spawnArg1.value == 0) {
        itemMenuDrawItemRow(arg1, arg0->rowTextX.signedValue, arg0->rowTextY.signedValue, item, arg0->colorRgb, 0);
    } else if (rec->attachSlot <= INVENTORY_ATTACHMENT_NONE) {
        itemMenuDrawItemRow(arg1, arg0->rowTextX.signedValue, arg0->rowTextY.signedValue, item, arg0->colorRgb, 1);
    } else {
        itemMenuDrawItemRow(arg1, arg0->rowTextX.signedValue, arg0->rowTextY.signedValue, item, arg0->colorRgb, 2);
    }
    if (item >= 0xA0 && item < 0xC0) {
        itemMenuDrawQuantity(arg1, arg0->rowTextX.signedValue, arg0->rowTextY.signedValue, rec->qty, arg0->colorRgb);
    }
    if (arg0->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        Gp_SelItemRec = rec;
        if (arg1->owner->state == 1) {
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
                sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                spawned = uiSpawnObject(&D_8010D6F4[4], arg1->owner->spawnArg1, 1, 1, arg1);
                if (spawned != NULL) {
                    spawned->panel.bounds.unsignedRect.x = arg1->panel.contentOriginX.unsignedValue + arg1->panel.contentLeft.unsignedValue + 0x14;
                    spawned->panel.bounds.unsignedRect.y = (arg1->panel.contentOriginY.unsignedValue + arg0->rowTextY.unsignedValue) - 0x14;
                    arg1->panel.control.word             = USER_INTERFACE_PANEL_INACTIVE;
                }
            } else if ((padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_TRIANGLE) != 0) && (item != 0)) {
                sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                uiSpawnObject(&D_8010EAB4[45], item, 1, 1, arg1);
                arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            idx   = arg1->owner->spawnArg1.value;
            item2 = inventoryGetRow(&Gp_MoveScanSrc + idx, Gp_InvLists[idx].selectedItemIndex, 0)->itemId;
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
                itemMenuSpawnNotice(arg1, item, 0, ITEM_MENU_NOTICE_RESULT_CONFIRM);
                arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            } else {
                Gp_ItemMoveWork->swapPartnerRow = arg0->currentItemIndex;
                arg1->result                    = 0x25;
            }
        }
    }
}

/// Returns whether the transfer context forbids moving or swapping this item.
///
/// itemId must index the live ordinary-item catalogue. Only battle-field mode
/// enforces ITEM_FLAG_NO_DISCARD; other mode values leave that flag unrestricted.
/// The current Acropolis container kind 0x703 independently retains the M93R.
/// Returns 0 or 1 and borrows the catalogue, save and context without changing them.
static inline s32 _itemMenuCheckTransferRestriction(s32 itemId, s32 battleFieldMode)
{
    s32 restricted;

    restricted = 0;
    if (Gp_ItemDescs[itemId].flags & ITEM_FLAG_NO_DISCARD) {
        restricted = battleFieldMode == ITEM_MENU_PANE_MODE_BATTLE_FIELD;
    }
    if ((Gp_MoveItemKey == ITEM_MENU_M93R_RESTRICTED_CONTAINER_KIND) && (itemId == ITEM_MENU_M93R_ITEM_ID) && (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage == GAME_STAGE_ACROPOLIS)) {
        restricted = 1;
    }
    return restricted;
}

/// Returns the borrowed item range selected by a transfer pane.
///
/// The pane task's first spawn word must already be normalized to index 0
/// (container) or 1 (carried items). The screen owns the copied descriptors;
/// their backing rows must remain live throughout the transfer screen.
static inline InventoryItemRange* _itemMenuGetPaneRange(const Task* task)
{
    return &Gp_MoveScanSrc + task->spawnArg1.value;
}

/// Visits pane dialogs, allowing the callback to detach the current child.
///
/// Saves the successor before the call and compares it to the current head
/// afterwards; the saved successor and pane owner must remain live.
static inline void _itemMenuVisitPaneChildren(const UiObject* object, UiObjectTaskFunc visitChild)
{
    Task* owningTask;
    Task* childTask;
    Task* nextSibling;
    Task* childHead;

    owningTask = object->owner;
    childTask  = owningTask->firstChild;
    if (childTask != NULL) {
        do {
            nextSibling = childTask->nextSibling;
            visitChild(childTask->spawnArg2.pointer, childTask);
            childHead = owningTask->firstChild;
            childTask = nextSibling;
            if (childHead == NULL) {
                break;
            }
        } while (childTask != childHead);
    }
}

/// Updates one inventory pane of the container/carried-item transfer screen.
///
/// spawnArg2 borrows its live task-owned UiObject; spawnArg1 selects pane 0
/// or 1, optionally with BATTLE_FIELD_FLAG. Initialization strips that flag,
/// records the title mode in status and enters BROWSING. The parent can put
/// the pane in CHOOSING_SWAP_PARTNER. Uses a shared list per pane, displaying
/// at most ten rows. Publishes cancel, cancel-swap or change-pane commands
/// and visits child dialogs while tolerating removal of the current child.
/// Requires live range backing, menu resources and writable GPU storage.
static void _itemMenuTransferPaneTask(Task* task)
{
    UiObject*           object;
    UiList*             list;
    InventoryItemRange* range;
    s32                 paneCapacity;
    s32                 rowsExceedCapacity;
    s32                 itemCount;
    s32                 panelControl;

    list           = &Gp_InvLists[(u8)task->spawnArg1.value];
    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    if (task->state == 0) {
        if (task->spawnArg1.value >= ITEM_MENU_PANE_BATTLE_FIELD_FLAG) {
            task->spawnArg1.value = task->spawnArg1.value & ITEM_MENU_PANE_INDEX_MASK;
            task->status          = ITEM_MENU_PANE_MODE_BATTLE_FIELD;
        } else {
            task->status = ITEM_MENU_PANE_MODE_ITEM_BOX;
        }
        {
            s32 rowCount;

            rowCount                            = _itemMenuGetPaneRange(task)->rowCount;
            list->itemCount                     = rowCount;
            list->visibleRowCount.unsignedValue = rowCount;
            if ((s8)rowCount > ITEM_MENU_PANE_VISIBLE_ROWS) {
                list->visibleRowCount.unsignedValue = ITEM_MENU_PANE_VISIBLE_ROWS;
            }
        }
        list->selectedItemIndex                   = 0;
        list->firstVisibleItemIndex.unsignedValue = 0;
        uiFitPanelToList(list, &(object)->panel);
        list->flags = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        task->state = task->state + 1;
    }

    if (task->spawnArg1.value == ITEM_MENU_PANE_CONTAINER) {
        if (task->status == ITEM_MENU_PANE_MODE_BATTLE_FIELD) {
            uiDrawPanelLabel(&(object)->panel, Gp_StrBattleField);
        } else {
            uiDrawPanelLabel(&(object)->panel, Gp_StrItemBox);
        }
    } else {
        uiDrawPanelLabel(&(object)->panel, Gp_StrPlayerItem);
    }
    uiRefreshListViewport(list, &(object)->panel);
    list->flags = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
    if (list->selectedItemIndex >= list->itemCount) {
        list->selectedItemIndex = list->itemCount - 1;
    }
    itemCount = list->itemCount;
    if (list->visibleRowCount.signedValue >= itemCount) {
        list->firstVisibleItemIndex.unsignedValue = 0;
    }
    if (list->itemCount != 0) {
        uiUpdateList(list, &object->panel);
    }

    // Preserve the overflow-highlight path; bounded ranges cannot exceed their capacity.
    range              = _itemMenuGetPaneRange(task);
    paneCapacity       = range->rowCount;
    rowsExceedCapacity = paneCapacity < inventoryCountOccupiedRows(range);
    if (rowsExceedCapacity != 0) {
        object->panel.style |= USER_INTERFACE_PANEL_SCREEN_BRIGHTEN;
    } else {
        object->panel.style &= ~USER_INTERFACE_PANEL_SCREEN_BRIGHTEN;
    }

    panelControl = object->panel.control.word;
    if (panelControl == USER_INTERFACE_PANEL_ACTIVE) {
        if (list->itemCount == 0) {
            uiEaseAndDrawCursor(&(object)->panel, object->panel.contentLeft.signedValue + 4, object->panel.contentTop.signedValue + 0xA);
        }
        if (task->state == ITEM_MENU_PANE_BROWSING) {
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
                object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                object->result             = USER_INTERFACE_RESULT_CANCEL;
            } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
                object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                object->result             = USER_INTERFACE_RESULT_CANCEL;
            } else if (padCheckButtons(0, PAD_BUTTON_QUERY_HELD_ANY, PAD_BUTTON_UP | PAD_BUTTON_DOWN) == 0) {
                if ((task->spawnArg1.value == ITEM_MENU_PANE_CONTAINER && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_RIGHT) != 0) || (task->spawnArg1.value == ITEM_MENU_PANE_CARRIED && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_LEFT) != 0) || padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_L2 | PAD_BUTTON_R2) != 0) {
                    sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
                    object->result = ITEM_MENU_RESULT_CHANGE_PANE;
                }
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel | Pad_MaskMenu) != 0) {
            object->result = ITEM_MENU_RESULT_CANCEL_SWAP;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_HELD_ANY, PAD_BUTTON_UP | PAD_BUTTON_DOWN) == 0) {
            if ((task->spawnArg1.value == ITEM_MENU_PANE_CONTAINER && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_RIGHT) != 0) || (task->spawnArg1.value == ITEM_MENU_PANE_CARRIED && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_LEFT) != 0) || padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_L2 | PAD_BUTTON_R2) != 0) {
                object->result = ITEM_MENU_RESULT_CHANGE_PANE;
            }
        }
    }

    _itemMenuVisitPaneChildren(object, _itemMenuHandlePaneChildResult);
}

void itemMenuDrawTransferMoveRow(UiList* list, UiObject* object)
{
    TextDrawReq       textRequest;
    s32               battleFieldMode;
    InventoryItemRow* selectedRow;
    Task*             rangeTask;
    Task*             actionTask;
    s32               paneIndex;
    s32               rowInputEnabled;
    s32               transferRestricted;
    s32               noticeId;
    s32               chooseQuantity;
    s32               quantity;
    s32               itemId;

    textRequest.x          = object->panel.contentOriginX.unsignedValue + list->rowTextX.unsignedValue;
    textRequest.y          = object->panel.contentOriginY.unsignedValue + list->rowTextY.unsignedValue;
    textRequest.otIndex    = object->panel.otIndex.signedValue + 1;
    textRequest.colorRgb   = list->colorRgb;
    textRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    textRequest.alignment  = TEXT_ALIGNMENT_LEFT;
    textRequest.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&textRequest, Gp_StrMove2);
    rowInputEnabled = list->rowInputEnabled;
    if ((rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) && (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0)) {
        noticeId       = -1;
        chooseQuantity = 0;
        paneIndex      = object->owner->spawnArg1.value;
        selectedRow    = inventoryGetRow((&Gp_MoveScanSrc + (paneIndex)), Gp_InvLists[paneIndex].selectedItemIndex, 0);
        itemId         = selectedRow->itemId;
        quantity       = selectedRow->qty;
        sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
        // Item-box ammunition uses a quantity selector; battlefield stacks move whole.
        if ((u32)(itemId - INVENTORY_CONSUMABLE_ITEM_FIRST) < (u32)INVENTORY_CONSUMABLE_ITEM_COUNT) {
            rangeTask = object->owner;
            if (rangeTask->status != 0) {
                if ((inventoryFindLastItemRowInRange(itemId, (&Gp_MoveScanSrc + (rangeTask->spawnArg1.value ^ 1))) == NULL) && (inventoryCanAddItem((&Gp_MoveScanSrc + (object->owner->spawnArg1.value ^ 1)), itemId) == 0)) {
                    noticeId = ITEM_MENU_NOTICE_NO_TRANSFER_SPACE;
                }
            } else if ((inventoryGetItemQuantity((&Gp_MoveScanSrc + (rangeTask->spawnArg1.value ^ 1)), itemId) != 0) || (inventoryCanAddItem((&Gp_MoveScanSrc + (object->owner->spawnArg1.value ^ 1)), itemId) != 0)) {
                chooseQuantity = 1;
            } else {
                noticeId = ITEM_MENU_NOTICE_NO_TRANSFER_SPACE;
            }
        } else if (inventoryCanAddItem((&Gp_MoveScanSrc + (object->owner->spawnArg1.value ^ 1)), itemId) != 0) {
            actionTask         = object->owner;
            battleFieldMode    = actionTask->parent->status;
            transferRestricted = _itemMenuCheckTransferRestriction(itemId, battleFieldMode);
            if (transferRestricted != 0) {
                noticeId = ITEM_MENU_NOTICE_CANNOT_MOVE_ITEM;
            } else if ((u32)(itemId - EQUIPMENT_WEAPON_ITEM_FIRST) < (u32)ITEM_MENU_EQUIPMENT_ITEM_COUNT) {
                if ((object->owner->spawnArg1.value != 1) || (itemId != (gPlayerStatus.weapon + (EQUIPMENT_WEAPON_ITEM_FIRST - 1)))) {
                    if (noticeId == -1) {
                        equipmentClearRemovableLoads(itemId);
                    }
                } else {
                    noticeId = ITEM_MENU_NOTICE_CANNOT_MOVE_EQUIPPED_ITEM;
                }
            } else if (((u32)(itemId - ITEM_MENU_ARMOR_ITEM_FIRST) < (u32)ITEM_MENU_EQUIPMENT_ITEM_COUNT) && (object->owner->spawnArg1.value == 1) && (itemId == (gPlayerStatus.armor + (ITEM_MENU_ARMOR_ITEM_FIRST - 1)))) {
                noticeId = ITEM_MENU_NOTICE_CANNOT_MOVE_EQUIPPED_ITEM;
            }
        } else {
            noticeId = ITEM_MENU_NOTICE_NO_TRANSFER_SPACE;
        }
        if (noticeId >= 0) {
            itemMenuSpawnNotice(object, noticeId, 0, ITEM_MENU_NOTICE_RESULT_DISMISS);
            object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            return;
        }
        // Show a preview or commit the whole selected row to the opposite pane.
        if (chooseQuantity == 1) {
            if (uiSpawnObject(&D_8010D6F4[5], itemId, 1, 1, object) != NULL) {
                object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            }
        } else {
            inventoryRemoveItemRow((&Gp_MoveScanSrc + (object->owner->spawnArg1.value)), selectedRow, quantity);
            inventoryGiveItem((&Gp_MoveScanSrc + (object->owner->spawnArg1.value ^ 1)), itemId, quantity);
            object->result = USER_INTERFACE_RESULT_CONFIRM;
        }
    }
}

void itemMenuDrawSwitchRow(UiList* list, UiObject* object)
{
    TextDrawReq             textRequest;
    s32                     rowInputEnabled;
    s32                     paneIndex;
    const InventoryItemRow* selectedRow;
    s32                     itemId;
    s32                     transferRestricted;
    s32                     battleFieldMode;
    Task*                   actionTask;
    const PlayerStatus*     player;

    textRequest.x          = object->panel.contentOriginX.unsignedValue + list->rowTextX.unsignedValue;
    textRequest.y          = object->panel.contentOriginY.unsignedValue + list->rowTextY.unsignedValue;
    textRequest.otIndex    = object->panel.otIndex.signedValue + 1;
    textRequest.colorRgb   = list->colorRgb;
    textRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    textRequest.alignment  = TEXT_ALIGNMENT_LEFT;
    textRequest.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&textRequest, Gp_StrSwitch);

    rowInputEnabled = list->rowInputEnabled;
    if (rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            paneIndex   = object->owner->spawnArg1.value;
            selectedRow = inventoryGetRow(&Gp_MoveScanSrc + paneIndex, Gp_InvLists[paneIndex].selectedItemIndex, 0);
            itemId      = selectedRow->itemId;
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);

            actionTask         = object->owner;
            battleFieldMode    = actionTask->parent->status;
            transferRestricted = _itemMenuCheckTransferRestriction(itemId, battleFieldMode);
            if (transferRestricted) {
                itemMenuSpawnNotice(object, ITEM_MENU_NOTICE_CANNOT_MOVE_ITEM, 0, ITEM_MENU_NOTICE_RESULT_DISMISS);
                object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            } else if (object->owner->spawnArg1.value == 1) {
                player = &gPlayerStatus;
                if ((itemId == player->weapon + (EQUIPMENT_WEAPON_ITEM_FIRST - 1)) || (itemId == player->armor + (ITEM_MENU_ARMOR_ITEM_FIRST - 1))) {
                    itemMenuSpawnNotice(object, ITEM_MENU_NOTICE_CANNOT_MOVE_EQUIPPED_ITEM, 0, ITEM_MENU_NOTICE_RESULT_DISMISS);
                    object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                } else {
                    object->result = ITEM_MENU_RESULT_BEGIN_SWAP;
                }
            } else {
                object->result = ITEM_MENU_RESULT_BEGIN_SWAP;
            }
        }
    }
}

/// Builds the Move, Switch and Use rows for the currently selected transfer item.
///
/// Borrows a writable action list and its task-owned object. Its owner carries
/// pane index 0/1 and the inherited battle-field mode; that pane's shared list
/// selects a valid inventory row. An empty/missing row offers only Switch.
/// Item-box consumables omit Switch; the supported recovery/use items add Use.
/// Writes at most three shared callbacks and sets both row counts to that size.
static void _itemMenuFillTransferActions(UiList* list, UiObject* object)
{
    const InventoryItemRow* selectedRow;
    s32                     itemId;
    s32                     actionCount;
    s32                     paneIndex;
    UiListRowCallback*      callbacks;
    Task*                   actionTask;
    InventoryItemRange*     range;

    actionTask  = object->owner;
    paneIndex   = actionTask->spawnArg1.value;
    range       = &Gp_MoveScanSrc + paneIndex;
    selectedRow = inventoryGetRow(range, Gp_InvLists[paneIndex].selectedItemIndex, 0);
    itemId      = INVENTORY_ITEM_NONE;
    if (selectedRow != NULL) {
        itemId = selectedRow->itemId;
    }
    if (itemId == INVENTORY_ITEM_NONE) {
        Gp_ItemActionFns[0] = itemMenuDrawSwitchRow;
        actionCount         = 1;
    } else {
        Gp_ItemActionFns[0] = itemMenuDrawTransferMoveRow;
        callbacks           = Gp_ItemActionFns;
        actionCount         = 1;
        if (((u32)(itemId - INVENTORY_CONSUMABLE_ITEM_FIRST) >= (u32)INVENTORY_CONSUMABLE_ITEM_COUNT) || (object->owner->status != 0)) {
            callbacks[1] = itemMenuDrawSwitchRow;
            actionCount  = 2;
        }
        if (((u32)(itemId - ITEM_MENU_ITEM_RECOVERY_FIRST) < (u32)ITEM_MENU_ITEM_RECOVERY_COUNT) || (itemId == ITEM_MENU_ITEM_COLA) || (itemId == ITEM_MENU_ITEM_MP_BOOST1) || (itemId == ITEM_MENU_ITEM_MP_BOOST2) || (itemId == ITEM_MENU_ITEM_PROTEIN_CAPSULE) || (itemId == ITEM_MENU_ITEM_RINGER)) {
            Gp_ItemActionFns[actionCount] = itemMenuDrawUseRow;
            actionCount                   = actionCount + 1;
        }
    }
    list->visibleRowCount.unsignedValue = actionCount;
    list->itemCount                     = actionCount;
}

/// Updates the selected item's transfer-action popup and forwards child results.
///
/// spawnArg2 borrows its live task-owned UiObject and spawnArg1 is pane 0/1.
/// On initialization inherits the pane's title mode, keeps the popup's bottom
/// at or above screen Y=100 pixels and builds the action rows. Menu returns
/// cancel in item-box mode and confirm in battle-field mode; Cancel returns
/// confirm. A child's dismiss becomes confirm; a child's confirm closes the
/// child and restores popup input. Uses the shared singleton action list.
static void _itemMenuTransferActionsTask(Task* task)
{
    Task*     childTask;
    UiObject* object;
    UiList*   list;
    UiObject* childObject;
    s32       childResult;
    Task*     parentTask;

    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    list           = &Gp_ItemActionList;
    if (task->state == 0) {
        parentTask = task->parent;
        if (parentTask != NULL) {
            task->status = parentTask->status;
        }
        if (((s16)object->panel.bounds.unsignedRect.y + (s16)object->panel.bounds.unsignedRect.h) > ITEM_MENU_ACTION_PANEL_BOTTOM) {
            object->panel.bounds.unsignedRect.y = ITEM_MENU_ACTION_PANEL_BOTTOM - object->panel.bounds.unsignedRect.h;
        }
        _itemMenuFillTransferActions(list, object);
        uiFitPanelToList(list, &(object)->panel);
        task->state = task->state + 1;
    }
    uiUpdateList(list, &object->panel);
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            if (object->owner->status != 0) {
                object->result = USER_INTERFACE_RESULT_CONFIRM;
            } else {
                object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                object->result             = USER_INTERFACE_RESULT_CANCEL;
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
            object->result = USER_INTERFACE_RESULT_CONFIRM;
        }
    }
    childTask = task->firstChild;
    if (childTask != NULL) {
        childObject = childTask->spawnArg2.pointer;
        childResult = childObject->result;
        switch (childResult) {
            case USER_INTERFACE_RESULT_CANCEL:
                object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                object->result             = childResult;
                break;
            case USER_INTERFACE_RESULT_DISMISS:
                object->result = USER_INTERFACE_RESULT_CONFIRM;
                break;
            case USER_INTERFACE_RESULT_CONFIRM:
                object->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
                uiStartTreeClosing(childObject, childObject->owner);
                break;
        }
    }
}

static const char Gp_StrBullet[] = "Bullet";

/// Transfers preview ammunition between carried and container stacks from port-zero input.
///
/// The caller gates this on an active panel. Requires live task/split storage,
/// pad for port zero, a positive acceleratedStep in rounds, and initial quantities
/// 0 <= loadedQty <= carriedQty <= stackLimit and 0 <= containerQty <= stackLimit.
/// Left/Right move one round, or acceleratedStep after 20 repeat ticks, unless
/// Up/Down is held. Up/L1/L2 and Down/R1/R2 move all available rounds toward the
/// container and carried side respectively, unless Left/Right is held.
/// Keeps the combined quantity and loaded minimum, sets LOADED/CAPACITY notices
/// on task->status, and leaves inventory and the initial-quantity snapshots intact.
/// Bulk transfers reaching capacity report CAPACITY even when no excess remains.
static inline void _itemMenuAdjustAmmoSplitQuantities(Task* task, _ItemMenuAmmoSplitWork* split, const PadState* pad, s32 acceleratedStep)
{
    s32 carriedBeforeStep;
    s32 loadedMinimum;
    s32 stepToContainer;
    s32 containerAfterMove;
    s32 containerLimit;
    s32 carriedAfterStep;
    s32 containerAfterStep;
    s32 carriedAfterClamp;
    s32 carriedLimit;
    s32 containerToMove;
    s32 moveAllLimit;
    s32 combinedQuantity;

    // Step between sides, clamping to stack capacity and the loaded minimum.
    if (padCheckButtons(0, PAD_BUTTON_QUERY_HELD_ANY, PAD_BUTTON_UP | PAD_BUTTON_DOWN) == 0) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_LEFT) != 0) {
            carriedBeforeStep = split->carriedQty;
            loadedMinimum     = split->loadedQty;
            if (loadedMinimum < carriedBeforeStep) {
                stepToContainer = 1;
                if (pad->directionRepeatTicks >= (u32)ITEM_MENU_AMMO_ACCELERATE_REPEAT_TICKS) {
                    stepToContainer = acceleratedStep;
                }
                split->containerQty += stepToContainer;
                split->carriedQty   -= stepToContainer;
                if (split->carriedQty < split->loadedQty) {
                    split->containerQty += split->carriedQty - split->loadedQty;
                    split->carriedQty    = split->loadedQty;
                }
                containerAfterMove = split->containerQty;
                containerLimit     = split->stackLimit;
                if (containerLimit < containerAfterMove) {
                    split->containerQty = containerLimit;
                    split->carriedQty   = split->carriedQty + (containerAfterMove - containerLimit);
                    task->status        = ITEM_MENU_AMMO_NOTICE_CAPACITY;
                }
            } else if (loadedMinimum > 0) {
                task->status = ITEM_MENU_AMMO_NOTICE_LOADED;
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_RIGHT) != 0) {
            s32 stepToCarried;

            stepToCarried = 1;
            if (pad->directionRepeatTicks >= (u32)ITEM_MENU_AMMO_ACCELERATE_REPEAT_TICKS) {
                stepToCarried = acceleratedStep;
            }
            split->containerQty = split->containerQty - stepToCarried;
            carriedAfterStep    = split->carriedQty + stepToCarried;
            split->carriedQty   = carriedAfterStep;
            containerAfterStep  = split->containerQty;
            if (containerAfterStep < 0) {
                split->carriedQty   = carriedAfterStep + containerAfterStep;
                split->containerQty = 0;
            }
            carriedAfterClamp = split->carriedQty;
            carriedLimit      = split->stackLimit;
            if (carriedLimit < carriedAfterClamp) {
                split->carriedQty   = carriedLimit;
                split->containerQty = split->containerQty + (carriedAfterClamp - carriedLimit);
                task->status        = ITEM_MENU_AMMO_NOTICE_CAPACITY;
            }
        }
    }
    // The perpendicular axis suppresses the bulk-transfer shortcuts.
    if (padCheckButtons(0, PAD_BUTTON_QUERY_HELD_ANY, PAD_BUTTON_RIGHT | PAD_BUTTON_LEFT) == 0) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_L2 | PAD_BUTTON_L1 | PAD_BUTTON_UP) != 0) {
            if (split->carriedQty > split->loadedQty) {
                s32 movableQuantity;

                movableQuantity  = split->containerQty + split->carriedQty;
                movableQuantity -= split->loadedQty;
                if (movableQuantity < split->stackLimit) {
                    split->carriedQty   = split->loadedQty;
                    split->containerQty = movableQuantity;
                } else {
                    split->containerQty = split->stackLimit;
                    split->carriedQty   = split->loadedQty + (movableQuantity - split->stackLimit);
                    task->status        = ITEM_MENU_AMMO_NOTICE_CAPACITY;
                }
            } else if (split->loadedQty > 0) {
                task->status = ITEM_MENU_AMMO_NOTICE_LOADED;
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_R2 | PAD_BUTTON_R1 | PAD_BUTTON_DOWN) != 0) {
            containerToMove = split->containerQty;
            if (containerToMove > 0) {
                moveAllLimit     = split->stackLimit;
                combinedQuantity = containerToMove + split->carriedQty;
                if (combinedQuantity < moveAllLimit) {
                    split->carriedQty   = combinedQuantity;
                    split->containerQty = 0;
                } else {
                    split->carriedQty   = moveAllLimit;
                    split->containerQty = combinedQuantity - split->stackLimit;
                    task->status        = ITEM_MENU_AMMO_NOTICE_CAPACITY;
                }
            }
        }
    }
}

/// Divides one ammunition stack between container and carried inventories.
///
/// spawnArg1 is a consumable id 0xA0..0xBF; spawnArg2 borrows the live
/// quantity-panel object. Allocates task-owned split work on initialization;
/// allocation failure returns DISMISS. Quantities count rounds, including
/// loaded carried rounds which remain immovable. The combined quantity must
/// be positive, the drawn bar width positive, both ranges valid, and loaded
/// rounds no greater than the carried total. Left/Right move one round (or
/// accelerated steps after 20 repeat ticks); Up/L1/L2 and Down/R1/R2 move all
/// movable rounds toward container and carried items respectively. Confirm
/// commits only the difference and returns DISMISS. Cancel dismisses without
/// committing; Menu returns CANCEL and deactivates input. Notice lifetime is
/// 188 callback ticks. Requires menu textures and writable GPU/OT storage.
static void _itemMenuAmmoSplitTask(Task* task)
{
    u8                      quantityText[0x20];
    s32                     carriedColorRgb;
    s32                     width;
    s32                     barInteriorWidth;
    s32                     barLeft;
    LINE_F2*                line;
    UiObject*               object;
    s16                     panelY;
    s16                     lineCoordinate;
    s32                     containerQuantity;
    s32                     loadedBarWidth;
    s32                     textY;
    s32                     splitWidth;
    s32                     caretX;
    s32                     panelControl;
    s32                     caretY;
    s32                     usableWidth;
    s32                     containerInitialQty;
    s32                     carriedInitialQty;
    s32                     negWidth;
    s32                     halfWidth;
    s32                     loadedQuantity;
    s32                     totalQty;
    s32                     carriedQuantity;
    s32                     repeatStep;
    s32                     transferQty;
    u8                      noticeStatus;
    s16                     noticeTicksRemaining;
    PadState*               pad;
    InventoryItemRange*     containerRange;
    InventoryItemRange*     carriedRange;
    _ItemMenuAmmoSplitWork* split;

    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    width          = (object->panel.contentRight.signedValue - object->panel.contentLeft.signedValue) - 0x50;
    uiDrawPanelLabel(&(object)->panel, Gp_StrBullet);
    // Snapshot both stacks; loaded carried rounds stay on the carried side.
    if (task->state == 0) {
        split = memCalloc(sizeof(*split), 0);
        if (split == NULL) {
            object->result = USER_INTERFACE_RESULT_DISMISS;
            return;
        }
        task->work                 = split;
        containerInitialQty        = inventoryGetConsumableStackQuantity(&Gp_MoveScanSrc, task->spawnArg1.value);
        split->containerQty        = containerInitialQty;
        split->containerInitialQty = containerInitialQty;
        carriedInitialQty          = inventoryGetConsumableStackQuantity(&Gp_MoveScanSrc + 1, task->spawnArg1.value);
        split->carriedQty          = carriedInitialQty;
        split->carriedInitialQty   = carriedInitialQty;
        split->loadedQty           = equipmentGetLoadedConsumableQuantity(&Gp_MoveScanSrc + 1, task->spawnArg1.value);
        uiSetPromptText(Gp_StrSetAmmoHelp, 0, 0);
        split->stackLimit = Gp_StackLimits[task->spawnArg1.value - INVENTORY_CONSUMABLE_ITEM_FIRST].maxHeld;
        task->state       = task->state + 1;
    }
    split = task->work;
    itemMenuDrawItemRow(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 0xF, task->spawnArg1.value, ITEM_MENU_NORMAL_COLOR_RGB, 0);
    task->status    = ITEM_MENU_AMMO_NOTICE_NONE;
    totalQty        = split->containerQty + split->carriedQty;
    carriedColorRgb = ITEM_MENU_NORMAL_COLOR_RGB;
    if (width < totalQty) {
        repeatStep = totalQty / width;
    } else {
        repeatStep = 1;
    }
    pad = gPadStates;
    if (pad->directionRepeatTicks != 0) {
        pad->directionRepeatTicks += gDisplayState.frameTicks * 2;
    }
    panelControl = object->panel.control.word;
    if (panelControl == USER_INTERFACE_PANEL_ACTIVE) {
        _itemMenuAdjustAmmoSplitQuantities(task, split, pad, repeatStep);

        // Apply only the net change; the preview has not touched inventory.
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            transferQty = split->containerQty - split->containerInitialQty;
            if (transferQty > 0) {
                containerRange = &Gp_MoveScanSrc;
                inventoryGiveItem(containerRange, task->spawnArg1.value, transferQty);
                inventoryConsumeFirstStack(containerRange + 1, task->spawnArg1.value, transferQty);
            } else if (transferQty < 0) {
                transferQty  = -transferQty;
                carriedRange = &Gp_MoveScanDst;
                inventoryGiveItem(carriedRange, task->spawnArg1.value, transferQty);
                inventoryConsumeFirstStack(carriedRange - 1, task->spawnArg1.value, transferQty);
            }
            object->result = USER_INTERFACE_RESULT_DISMISS;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
            object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            object->result             = USER_INTERFACE_RESULT_CANCEL;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
            object->result = USER_INTERFACE_RESULT_DISMISS;
        }
    }
    // Draw the split boundary and the immovable loaded-round segment.
    carriedQuantity = split->carriedQty;
    if ((carriedQuantity == split->loadedQty) && (carriedQuantity > 0)) {
        carriedColorRgb = ITEM_MENU_LOADED_AMMO_COLOR_RGB;
    }
    containerQuantity = split->containerQty;
    usableWidth       = width - 2;
    barInteriorWidth  = usableWidth;
    panelY            = object->panel.contentTop.signedValue;
    splitWidth        = ((s32)(containerQuantity * usableWidth) / (s32)(containerQuantity + split->carriedQty)) + 1;
    textY             = panelY + 0x20;
    textDrawUiLine(object, object->panel.contentLeft.signedValue + 0x20, textY, textItoaUnsigned(quantityText, (u32)containerQuantity), ITEM_MENU_NORMAL_COLOR_RGB, TEXT_DRAW_OUTLINED,
                   TEXT_ALIGNMENT_RIGHT);
    textDrawUiLine(object, object->panel.contentRight.signedValue - 6, textY, textItoaUnsigned(quantityText, (u32)split->carriedQty), carriedColorRgb, TEXT_DRAW_OUTLINED,
                   TEXT_ALIGNMENT_RIGHT);
    caretY    = panelY + 0x16;
    negWidth  = -width;
    halfWidth = negWidth / 2;
    caretX    = halfWidth + splitWidth;
    barLeft   = halfWidth;
    uiDrawFlatCaret(&(object)->panel, caretX, caretY, ITEM_MENU_NORMAL_COLOR_RGB, USER_INTERFACE_CARET_DOWN);
    uiDrawFlatCaret(&(object)->panel, caretX, panelY + 0x1E, ITEM_MENU_NORMAL_COLOR_RGB, USER_INTERFACE_CARET_UP);
    line                              = gGpuPrimCursor;
    gGpuPrimCursor                    = line + 1;
    GPU_PRIMITIVE_COLOR_WORD(line, 0) = GPU_PACK_COLOR_WORD(0x60, 0x60, 0x60, 0);
    lineCoordinate                    = object->panel.contentOriginX.unsignedValue + caretX;
    line->x1                          = lineCoordinate;
    line->x0                          = lineCoordinate;
    line->y0                          = (object->panel.contentOriginY.unsignedValue + textY) - 0xA;
    lineCoordinate                    = (object->panel.contentOriginY.unsignedValue + textY) - 2;
    setLineF2(line);
    line->y1 = lineCoordinate;
    addPrim(gGpuCurrentOt + object->panel.otIndex.signedValue + 1, line);
    loadedQuantity = split->loadedQty;
    if (loadedQuantity > 0) {
        loadedBarWidth = ((s32)(loadedQuantity * barInteriorWidth) / (s32)(split->containerQty + split->carriedQty)) + 2;
        if (loadedBarWidth > 0) {
            uiFillRectInterior(&(object)->panel, (barLeft + width) - loadedBarWidth, caretY, loadedBarWidth, 8, (u32)ITEM_MENU_LOADED_AMMO_COLOR_RGB);
        }
    }
    uiDrawRecessedRect(&object->panel, (s32)-width / 2, textY - 0xA, width, 8, ITEM_MENU_AMMO_BAR_BACKGROUND_RGB);
    noticeStatus = task->status;
    if (noticeStatus == ITEM_MENU_AMMO_NOTICE_LOADED) {
        task->killCountdown = ITEM_MENU_AMMO_NOTICE_TICKS;
        uiSetPromptText(Gp_StrAmmoLocked, 0, 0);
    } else if (noticeStatus == ITEM_MENU_AMMO_NOTICE_CAPACITY) {
        task->killCountdown = ITEM_MENU_AMMO_NOTICE_TICKS;
        uiSetPromptText(Gp_StrMaxCapacity, 0, 0);
    } else if (task->killCountdown > 0) {
        noticeTicksRemaining = (u16)task->killCountdown - 1;
        task->killCountdown  = noticeTicksRemaining;
        if (noticeTicksRemaining == 0) {
            uiSetPromptText(Gp_StrSetAmmoHelp, 0, 0);
        }
    }
}

/* After Gp_StrBullet from _itemMenuAmmoSplitTask so the overlay .rodata stays packed. */
static const _ItemMenuPromptTexts Gp_ItemPromptTexts = { Gp_StrAll, Gp_StrSelect, Gp_StrDiscard, Gp_StrEnd };
/// Vector template used by `Gp_ItemPickupTilt`.
static const VECTOR D_80093DB0 = { 0, -100, 0, 0 };

/// Clears removable weapon-load quantities whose consumable is no longer carried.
///
/// Scans every carried weapon row and clears only primary/secondary quantities,
/// keeping their selected ids. Empty/unavailable selections and built-in Battery
/// or Fuel supply ids are exempt even though they have no carried stack. The
/// live carried range and its backing rows must be valid; saved loads remain
/// owned by the live save. These exemptions correspond to EquipmentWeaponSupply.
static inline void _equipmentClearOrphanedWeaponLoads(void)
{
    InventoryItemRange*  carriedRange;
    InventoryItemRow*    row;
    EquipmentWeaponLoad* load;
    s32                  rowIndex;
    s32                  loadedItemId;

    carriedRange = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    row          = inventoryGetRangeTable(carriedRange);
    rowIndex     = 0;
    row          = &row[carriedRange->firstRow];
    if (carriedRange->rowCount != 0) {
        do {
            if ((u8)(row->itemId + EQUIPMENT_WEAPON_ITEM_FIRST) < ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems)) {
                load         = equipmentGetWeaponLoad(row->itemId);
                loadedItemId = load->primaryItemId;

                if ((loadedItemId != INVENTORY_ITEM_NONE) && (loadedItemId != EQUIPMENT_SUPPLY_HYPERVELOCITY_BATTERY)) {
                    if (inventoryGetItemQuantity(carriedRange, loadedItemId) == 0) {
                        load->primaryQty = 0;
                    }
                }
                loadedItemId = load->secondaryItemId;

                if ((loadedItemId != INVENTORY_ITEM_NONE) && (loadedItemId != EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE) && (loadedItemId != EQUIPMENT_SUPPLY_P229_MP5_BATTERY) && (loadedItemId != EQUIPMENT_SUPPLY_HAMMER_BATTERY) &&
                    (loadedItemId != EQUIPMENT_SUPPLY_PYKE_FUEL) && (loadedItemId != EQUIPMENT_SUPPLY_JAVELIN_BATTERY)) {
                    if (inventoryGetItemQuantity(carriedRange, loadedItemId) == 0) {
                        load->secondaryQty = 0;
                    }
                }
            }
            rowIndex++;
            row++;
        } while (rowIndex < carriedRange->rowCount);
    }
}

/// Draws and handles the All, Select and Discard transfer-exit commands.
///
/// Borrows the shared list and its task-owned object; currentItemIndex is 0..2.
/// All is skipped when spawnArg1 is zero (not all container rows fit). All
/// publishes MOVE_ALL, Select returns CONFIRM, and Discard clears orphaned
/// weapon-load quantities before publishing DISCARD_AND_EXIT. Cancel first
/// selects Discard and marks actionResult to suppress same-frame activation;
/// a subsequent cancel on that row performs Discard.
static void _itemMenuTransferExitRow(UiList* list, UiObject* object)
{
    _ItemMenuPromptTexts labels;
    s32                  rowIndex;

    labels = Gp_ItemPromptTexts;
    if (list->currentItemIndex == ITEM_MENU_PROMPT_ALL) {
        if (object->owner->spawnArg1.value == 0) {
            list->colorRgb = uiGetTextColor(object, USER_INTERFACE_TEXT_COLOR_DIMMED);
            if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
                list->navigationStep  = USER_INTERFACE_LIST_STEP_NEXT;
                list->actionResult    = USER_INTERFACE_LIST_ACTION_SKIP_ROW;
                list->rowInputEnabled = USER_INTERFACE_LIST_ROW_INACTIVE;
            }
        }
    }
    rowIndex = list->currentItemIndex;
    textDrawUiLine(object, list->rowTextX.signedValue, list->rowTextY.signedValue, labels.label[rowIndex], list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);

    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (list->currentItemIndex == ITEM_MENU_PROMPT_DISCARD) {
            if (list->actionResult == ITEM_MENU_LIST_ACTION_CANCEL_HANDLED) {
                list->actionResult = USER_INTERFACE_RESULT_NONE;
                return;
            }
        }
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            switch (list->currentItemIndex) {
                case ITEM_MENU_PROMPT_ALL:
                    sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                    object->result = ITEM_MENU_RESULT_MOVE_ALL;
                    break;
                case ITEM_MENU_PROMPT_SELECT:
                    sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
                    object->result = USER_INTERFACE_RESULT_CONFIRM;
                    break;
                case ITEM_MENU_PROMPT_DISCARD:
                    sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
                    _equipmentClearOrphanedWeaponLoads();
                    object->result = ITEM_MENU_RESULT_DISCARD_AND_EXIT;
                    break;
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
            if (list->selectedItemIndex == ITEM_MENU_PROMPT_DISCARD) {
                _equipmentClearOrphanedWeaponLoads();
                object->result = ITEM_MENU_RESULT_DISCARD_AND_EXIT;
            } else {
                list->actionResult      = ITEM_MENU_LIST_ACTION_CANCEL_HANDLED;
                list->selectedItemIndex = ITEM_MENU_PROMPT_DISCARD;
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
        if (room != 3) {
            extra->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        }
    } else if (stageAreaKey == GAME_LOCATION_KEY(4, 20, 0, 0)) {
        if (room != 0x11) {
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
        if (areaGetCurrentObjectState((u8)enemy->placeKey) != 2) {
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
                displayQueueModeTask(taskGetDesc(1, 0x26), 0, arg0->spawnArg2.value, STAGE_ENTRY_RELOAD);
            } else {
                displayQueueModeTask(taskGetDesc(1, 0x26), 0, arg0->spawnArg2.value, STAGE_ENTRY_RELOAD);
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

/// Visits the task-owned UI objects in a parent's circular child ring.
///
/// Borrows a live object and owner, with a closed ring of live child tasks whose
/// spawnArg2 pointers are their UI objects. The callback must be non-NULL while
/// children exist. It may detach or release the current child, but must keep the
/// saved successor live if the ring remains nonempty, preserve traversal order,
/// and keep the parent owner live. Inserting or reordering children is unsupported.
/// Stops at an empty ring or when the saved successor is the current head;
/// detaching the head can therefore end this walk before visiting its siblings.
static void _uiVisitChildObjects(const UiObject* object, UiObjectTaskFunc visitChild)
{
    Task* owningTask;
    Task* childTask;
    Task* nextSibling;
    Task* childHead;

    owningTask = object->owner;
    childTask  = owningTask->firstChild;
    if (childTask != NULL) {
        do {
            // The callback can detach this node and move or clear the ring's head.
            nextSibling = childTask->nextSibling;
            visitChild(childTask->spawnArg2.pointer, childTask);
            childHead = owningTask->firstChild;
            childTask = nextSibling;
            if (childHead == NULL) {
                break;
            }
        } while (childTask != childHead);
    }
}

/// Tests whether the current transfer context forbids moving or swapping an item.
///
/// itemId must index the ordinary item catalogue. battleFieldMode == 1 enforces
/// NO_DISCARD; other values do not. Independently, the Acropolis container kind
/// 0x703 keeps the M93R in place. Returns 0 or 1 without changing inventory.
static s32 _itemMenuIsTransferRestricted(s32 itemId, s32 battleFieldMode)
{
    return _itemMenuCheckTransferRestriction(itemId, battleFieldMode);
}

/// Handles a child dialog's result for its inventory pane.
///
/// Borrows the live child and its owning task, whose parent owns the pane.
/// Cancel closes the child: battle-field mode resumes pane input, while item-box
/// mode leaves it inactive and forwards cancel. Confirm closes the child and resumes
/// pane input without forwarding a result. Begin-swap closes and forwards;
/// move-all and discard-and-exit forward without closing. Other results do
/// nothing. Closing detaches the child and requests its animation; it does not
/// immediately release the child object or task.
static void _itemMenuHandlePaneChildResult(UiObject* child, Task* childTask)
{
    UiObject* pane;

    pane = childTask->parent->spawnArg2.pointer;
    switch (child->result) {
        case USER_INTERFACE_RESULT_CANCEL:
            if (pane->owner->status) {
                pane->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
                uiStartTreeClosing(child, child->owner);
            } else {
                uiStartTreeClosing(child, child->owner);
                pane->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                pane->result             = USER_INTERFACE_RESULT_CANCEL;
            }
            break;
        case USER_INTERFACE_RESULT_CONFIRM:
            pane->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
            uiStartTreeClosing(child, child->owner);
            break;
        case ITEM_MENU_RESULT_BEGIN_SWAP:
            uiStartTreeClosing(child, child->owner);
            pane->result = ITEM_MENU_RESULT_BEGIN_SWAP;
            break;
        case ITEM_MENU_RESULT_MOVE_ALL:
        case ITEM_MENU_RESULT_DISCARD_AND_EXIT:
            pane->result = child->result;
            break;
    }
}

/// Updates the All/Select/Discard menu shown before leaving the transfer screen.
///
/// spawnArg2 borrows the live task-owned UiObject. spawnArg1 is whether every
/// container row fits in the carried range: zero starts on Select, nonzero on
/// All. Uses a shared singleton list and one row callback for all three rows.
static void _itemMenuTransferExitMenuTask(Task* task)
{
    UiObject* object;
    UiList*   list;

    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    list           = &Gp_ItemMenuList;
    if (task->state == 0) {
        uiFitPanelToList(list, &(object)->panel);
        if (task->spawnArg1.value == 0) {
            list->selectedItemIndex = ITEM_MENU_PROMPT_SELECT;
        } else {
            list->selectedItemIndex = ITEM_MENU_PROMPT_ALL;
        }
        list->flags  = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        task->state += 1;
    }
    uiUpdateList(list, &object->panel);
}

/// Draws the holder prompt's first two encoded text lines in fixed outlined grey.
///
/// Content-relative placement is left+2, top+15 and top+30 pixels. Borrows the
/// live object and text for this call, retaining neither. Text and drawing
/// resources must satisfy textDrawUiLine and textSkipLines; an initial N/n also
/// needs a readable predecessor byte. Hidden panels still scan for the second
/// line but emit no packets. Fewer than two lines draws an empty second line.
static inline void _itemMenuDrawHolderPromptLines(const UiObject* object, const u8* promptText)
{
    enum { ITEM_MENU_HOLDER_PROMPT_INSET_X  = 2,
           ITEM_MENU_HOLDER_PROMPT_FIRST_Y  = 15,
           ITEM_MENU_HOLDER_PROMPT_SECOND_Y = 30 };
    const u8* secondLine;

    textDrawUiLine(object, object->panel.contentLeft.signedValue + ITEM_MENU_HOLDER_PROMPT_INSET_X, object->panel.contentTop.signedValue + ITEM_MENU_HOLDER_PROMPT_FIRST_Y, promptText, ITEM_MENU_NORMAL_COLOR_RGB, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    secondLine = textSkipLines(promptText, 1);
    textDrawUiLine(object, object->panel.contentLeft.signedValue + ITEM_MENU_HOLDER_PROMPT_INSET_X, object->panel.contentTop.signedValue + ITEM_MENU_HOLDER_PROMPT_SECOND_Y, secondLine, ITEM_MENU_NORMAL_COLOR_RGB, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
}

/// Publishes the transfer screen's prompt panel and draws two text lines.
///
/// spawnArg2 borrows its live task-owned UiObject, published as Wip_UiHolder
/// on initialization. spawnArg1 is NULL or borrowed encoded text, kept live
/// while drawn and replaceable through uiSetPromptText. Uses fixed normal RGB
/// and outlined left-aligned text at contentLeft+2, contentTop+15/+30 pixels.
/// Text must satisfy textDrawUiLine and textSkipLines, including a readable
/// predecessor byte when the first byte is N/n. Requires loaded text resources
/// and writable GPU/OT storage; clearing the holder belongs to the parent.
static void _itemMenuHolderPromptTask(Task* task)
{
    UiObject* object;
    const u8* promptText;

    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    if (task->state == 0) {
        Wip_UiHolder = object;
        task->state += 1;
    }
    promptText = task->spawnArg1.pointer;
    if (promptText != 0) {
        _itemMenuDrawHolderPromptLines(object, promptText);
    }
}

/// Starts a placed container's lid action and retains its CAP completion request.
///
/// The message table dispatches CAP_ACTION_MESSAGE_REQUEST with a writable
/// request; messageId and the second payload are ignored. spawnArg2 borrows
/// the placed Enemy. Records action-requested status and retains request in
/// extraState until the lid animation completes, so its complete storage must
/// stay live across frames. If the place's two-bit state is already 2, sets
/// done immediately. Leaves accepted unchanged and returns zero.
static s32 _itemPickupHandleLidActionRequest(Task* task, s32 messageId, CapActionRequest* request, s32 unused)
{
    enum { ITEM_PICKUP_LID_ACTION_REQUESTED = 1,
           ITEM_PICKUP_PLACE_COLLECTED      = 2 };
    s32          actionRequested;
    const Enemy* enemy;

    enemy                    = task->spawnArg2.pointer;
    actionRequested          = ITEM_PICKUP_LID_ACTION_REQUESTED;
    task->status             = actionRequested;
    task->extraState.pointer = request;
    if (areaGetCurrentObjectState((u8)enemy->placeKey) == ITEM_PICKUP_PLACE_COLLECTED) {
        request->done = actionRequested;
    }
    return 0;
}

void itemPickupPublishPlacedObjectTask(Task* task)
{
    const Enemy* enemy = task->spawnArg2.pointer;
    s32          packQuantity;

    // Publish the place flag index and kind before selecting pickup quantity.
    Gp_PubItemId  = (u8)enemy->placeKey;
    Gp_PubItemLoc = enemy->workType;
    if (enemy->workType < INVENTORY_CONSUMABLE_ITEM_FIRST) {
        if (Gp_PubItemLoc >= ITEM_MENU_ARMOR_ITEM_FIRST && Gp_PubItemLoc < EQUIPMENT_WEAPON_ITEM_FIRST) {
            if (inventoryIsItemLimitReached(Gp_PubItemLoc) != 0) {
                Gp_PubItemLoc = ITEM_MENU_DUPLICATE_ARMOR_ITEM_ID;
            }
        } else if (Gp_PubItemLoc >= EQUIPMENT_WEAPON_ITEM_FIRST && Gp_PubItemLoc < INVENTORY_CONSUMABLE_ITEM_FIRST) {
            if (inventoryIsItemLimitReached(Gp_PubItemLoc) != 0) {
                Gp_PubItemLoc = ITEM_MENU_DUPLICATE_WEAPON_ITEM_ID;
            }
        }
        Gp_PubItemQty   = 1;
        Gp_PubItemReady = 1;
    } else {
        packQuantity    = Gp_StackLimits[Gp_PubItemLoc - INVENTORY_CONSUMABLE_ITEM_FIRST].packQty;
        Gp_PubItemReady = 1;
        Gp_PubItemQty   = packQuantity;
    }
    displaySetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
    Wip_UiHolder        = NULL;
    task->killCountdown = 1;
    task->state         = task->state + 1;
}

/// Queues a screen-sized subtractive overlay for a display transition.
///
/// `fadeStep` borrows a signed counter in 0..8 for this call. The tile's RGB
/// darkness is min(step * 32 + 31, 255); steps seven and eight both draw black.
/// `yOffsetPixels` is a signed pixel offset subtracted from the centered
/// tile's Y coordinate to compensate the display's vertical shake.
/// `otIndex` counts tags in `gGpuCurrentOt`; the caller selects 0, 59 or 63.
///
/// Requires space for one TILE and one DR_TPAGE at the word-aligned
/// `gGpuPrimCursor` and a writable selected OT tag. The packets and ordering
/// table remain borrowed until GPU completion; no capacity check is made.
static inline void _fadeQueueDisplayTransitionOverlay(const s16* fadeStep, s32 yOffsetPixels, s32 otIndex)
{
    s32       darkness;
    TILE*     fadeTile;
    DR_TPAGE* blendCommand;

    fadeTile       = gGpuPrimCursor;
    gGpuPrimCursor = fadeTile + 1;
    setTile(fadeTile);
    setSemiTrans(fadeTile, true);
    fadeTile->x0 = -FADE_DISPLAY_WIDTH_PIXELS / 2;
    fadeTile->y0 = -FADE_DISPLAY_HEIGHT_PIXELS / 2 - yOffsetPixels;
    fadeTile->w  = FADE_DISPLAY_WIDTH_PIXELS;
    fadeTile->h  = FADE_DISPLAY_HEIGHT_PIXELS;
    if (*fadeStep < FADE_DISPLAY_STEPS) {
        darkness     = (*fadeStep << FADE_DISPLAY_INTENSITY_SHIFT) + FADE_DISPLAY_MIN_GREY;
        fadeTile->b0 = darkness;
        fadeTile->g0 = darkness;
        fadeTile->r0 = darkness;
    } else {
        darkness     = FADE_DISPLAY_MAX_GREY;
        fadeTile->b0 = darkness;
        fadeTile->g0 = darkness;
        fadeTile->r0 = darkness;
    }

    // OT insertion prepends packets: link the draw mode last so it runs first.
    addPrim(&gGpuCurrentOt[otIndex], fadeTile);
    blendCommand   = gGpuPrimCursor;
    gGpuPrimCursor = blendCommand + 1;
    setlen(blendCommand, ARRAY_SIZE(blendCommand->code));
    blendCommand->code[0] = FADE_DISPLAY_DRAW_MODE;
    addPrim(&gGpuCurrentOt[otIndex], blendCommand);
}

void fadeDisplayTransitionTask(Task* task)
{
    s32 darkening;
    s32 yOffsetPixels;
    s32 otIndex;

    darkening = 0;
    if (task->state == 0) {
        if (task->spawnArg1.value == FADE_DISPLAY_REVEAL_FULL) {
            displaySetFrameTiming(DISPLAY_TIMING_TWO_VBLANKS);
            gDisplayState.control.flags.flipMode = DISPLAY_FLIP_FULL;
            task->killCountdown                  = FADE_DISPLAY_STEPS - 1;
        } else if ((task->spawnArg1.value == FADE_DISPLAY_REVEAL_WORLD) || (task->spawnArg1.value == FADE_DISPLAY_REVEAL_TRANSITION)) {
            task->killCountdown = FADE_DISPLAY_STEPS;
        } else {
            task->killCountdown = 0;
        }
        task->state = task->state + 1;
    }

    // Transition reveal holds black until its image strips are ready.
    if (task->spawnArg1.value == FADE_DISPLAY_REVEAL_TRANSITION) {
        if (gDisplayState.control.flags.imageSource == DISPLAY_IMAGE_TRANSITION_STRIPS) {
            task->killCountdown--;
        } else {
            displaySetFrameTiming(DISPLAY_TIMING_TWO_VBLANKS);
        }
    } else if ((task->spawnArg1.value == FADE_DISPLAY_REVEAL_FULL) || (task->spawnArg1.value == FADE_DISPLAY_REVEAL_WORLD)) {
        darkening = 0;
        task->killCountdown--;
    } else {
        darkening            = 1;
        task->killCountdown += darkening;
    }

    // World rendering moves with screen shake; the task overlays stay centered.
    yOffsetPixels = 0;
    if (task->spawnArg1.value == FADE_DISPLAY_REVEAL_WORLD) {
        yOffsetPixels = gDisplayState.vramYOffset;
        otIndex       = FADE_DISPLAY_WORLD_OT;
    } else if (task->spawnArg1.value == FADE_DISPLAY_CLEAR_IMAGE) {
        otIndex = FADE_DISPLAY_CLEAR_IMAGE_OT;
    } else {
        otIndex = FADE_DISPLAY_TASK_OT;
    }

    _fadeQueueDisplayTransitionOverlay(&task->killCountdown, yOffsetPixels, otIndex);

    // Queue the final overlay before changing presentation and releasing the task.
    if ((darkening == 0) && (task->killCountdown <= 0)) {
        if (task->spawnArg1.value == FADE_DISPLAY_REVEAL_TRANSITION) {
            displaySetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
        }
        taskKill(task);
    } else if (darkening == 1) {
        if (task->killCountdown >= FADE_DISPLAY_STEPS) {
            if (task->spawnArg1.value == FADE_DISPLAY_CLEAR_IMAGE) {
                gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
            } else {
                gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
                displaySetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
            }
            taskKill(task);
        }
    }
}
