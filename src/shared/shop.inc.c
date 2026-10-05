#include "shop.h"

#include <psyq/sys/types.h>

#include "common.h"
#include "types.h"

#include "gameplay/inventory.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/ui_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "rooms/shop_tier.h"

/// Work block of the shop's item-list panel, parked in `Task::work`.
///
/// The panel's task allocates it on its first frame and the task's teardown
/// frees it. The list's row callback and the builders reach `rowIds` through
/// the owning task and index it by the list's item index; `list.itemCount` is
/// the number of ids in use. Nothing checks an append against the capacity.
typedef struct {
    UiList list;         // List control the panel is drawn from; its `itemCount` counts the `rowIds` in use
    u16    rowIds[0x40]; // What each row offers: an item id, or 0xFFFE for the "Batteries/Fuel" recharge service
} _ShopItemListWork;
STATIC_ASSERT_SIZEOF(_ShopItemListWork, 0xA4);

static void Shop_ItemListTask(Task* task);
static void Shop_SessionTask(Task* task);

static u16* Shop_SelectStock(s32 mode);
static void Shop_ItemRow(UiList* prompt, UiObject* obj);
static void Shop_AddItem(UiList* list, UiObject* obj, s32 item);
static void Shop_BuildItemList(UiList* list, UiObject* obj);
static void Shop_CategoryRow(UiList* prompt, UiObject* obj);
static void Shop_CategoryListTask(Task* task);
static void Shop_BalanceTask(Task* task);
static void Shop_BuyRow(UiList* prompt, UiObject* obj);
static void Shop_NoticeTask(Task* task);
static void Shop_ChargeTask(Task* task);
static void Shop_PreviewTask(Task* task);
static void Shop_QuantityTask(Task* task);
static void Shop_MessageRow(UiList* prompt, UiObject* obj);
static void Shop_BuyPromptTask(Task* task);

/// Returns the 0xFFFF-terminated item id list the shop list starts from. The
/// low halfword of `mode` picks a group of lists (0x20, 0x21, 0x30-0x33, 0x40
/// or any other value) and the high halfword one of the group's four;
/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode` 2 and above has groups of its own. A high halfword
/// above 3 falls through the 0x30-0x33 groups in turn and on into 0x20's;
/// every other miss returns `Shop_Data_80181AD4`.
static u16* Shop_SelectStock(s32 mode)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode < 2) {
        switch ((u16)mode) {
            case 0x30:
                switch ((u32)mode >> 16) {
                    case 0:
                        return Shop_Data_801816D8;
                    case 1:
                        return Shop_Data_801816F0;
                    case 2:
                        return Shop_Data_80181704;
                    case 3:
                        return Shop_Data_8018170C;
                }
            case 0x31:
                switch ((u32)mode >> 16) {
                    case 0:
                        return Shop_Data_80181720;
                    case 1:
                        return Shop_Data_8018173C;
                    case 2:
                        return Shop_Data_8018174C;
                    case 3:
                        return Shop_Data_80181758;
                }
            case 0x32:
                switch ((u32)mode >> 16) {
                    case 0:
                        return Shop_Data_80181770;
                    case 1:
                        return Shop_Data_8018178C;
                    case 2:
                        return Shop_Data_801817A0;
                    case 3:
                        return Shop_Data_801817A8;
                }
            case 0x33:
                switch ((u32)mode >> 16) {
                    case 0:
                        return Shop_Data_801817BC;
                    case 1:
                        return Shop_Data_801817DC;
                    case 2:
                        return Shop_Data_801817EC;
                    case 3:
                        return Shop_Data_801817F8;
                }
            case 0x20:
                switch ((u32)mode >> 16) {
                    case 0:
                        return Shop_Data_80181620;
                    case 1:
                        return Shop_Data_80181630;
                    case 2:
                        return Shop_Data_80181640;
                    case 3:
                        return Shop_Data_80181648;
                }
                break;
            case 0x21:
                switch ((u32)mode >> 16) {
                    case 0:
                        return Shop_Data_80181694;
                    case 1:
                        return Shop_Data_801816AC;
                    case 2:
                        return Shop_Data_801816C0;
                    case 3:
                        return Shop_Data_801816C8;
                }
                break;
            case 0x40:
                switch ((u32)mode >> 16) {
                    case 0:
                        return Shop_Data_80181658;
                    case 1:
                        return Shop_Data_80181668;
                    case 2:
                        return Shop_Data_80181678;
                    case 3:
                        return Shop_Data_80181680;
                }
                break;
            default:
                switch ((u32)mode >> 16) {
                    case 0:
                        return Shop_Data_801815F8;
                    case 1:
                        return Shop_Data_80181600;
                    case 2:
                        return Shop_Data_80181608;
                    case 3:
                        return Shop_Data_80181610;
                }
                break;
        }
    } else {
        switch ((u16)mode) {
            case 0x30:
                switch ((u32)mode >> 16) {
                    case 0:
                        return Shop_Data_801818A4;
                    case 1:
                        return Shop_Data_801818B0;
                    case 2:
                        return Shop_Data_801818B8;
                    case 3:
                        return Shop_Data_801818C4;
                }
            case 0x31:
                switch ((u32)mode >> 16) {
                    case 0:
                        return Shop_Data_801818D0;
                    case 1:
                        return Shop_Data_801818DC;
                    case 2:
                        return Shop_Data_801818E0;
                    case 3:
                        return Shop_Data_801818EC;
                }
            case 0x32:
                switch ((u32)mode >> 16) {
                    case 0:
                        return Shop_Data_801818F8;
                    case 1:
                        return Shop_Data_80181904;
                    case 2:
                        return Shop_Data_8018190C;
                    case 3:
                        return Shop_Data_80181918;
                }
            case 0x33:
                switch ((u32)mode >> 16) {
                    case 0:
                        return Shop_Data_80181924;
                    case 1:
                        return Shop_Data_80181930;
                    case 2:
                        return Shop_Data_80181938;
                    case 3:
                        return Shop_Data_80181944;
                }
            case 0x20:
                switch ((u32)mode >> 16) {
                    case 0:
                        return Shop_Data_80181830;
                    case 1:
                        return Shop_Data_80181838;
                    case 2:
                        return Shop_Data_80181840;
                    case 3:
                        return Shop_Data_80181848;
                }
                break;
            case 0x21:
                switch ((u32)mode >> 16) {
                    case 0:
                        return Shop_Data_8018187C;
                    case 1:
                        return Shop_Data_80181888;
                    case 2:
                        return Shop_Data_80181890;
                    case 3:
                        return Shop_Data_80181898;
                }
                break;
            case 0x40:
                switch ((u32)mode >> 16) {
                    case 0:
                        return Shop_Data_80181854;
                    case 1:
                        return Shop_Data_8018185C;
                    case 2:
                        return Shop_Data_80181868;
                    case 3:
                        return Shop_Data_80181870;
                }
                break;
            default:
                switch ((u32)mode >> 16) {
                    case 0:
                        return Shop_Data_80181810;
                    case 1:
                        return Shop_Data_80181814;
                    case 2:
                        return Shop_Data_80181818;
                    case 3:
                        return Shop_Data_80181820;
                }
                break;
        }
    }
    return Shop_Data_80181AD4;
}

/// Draws one row of the shop list and handles its input, recording the row's
/// id as the cursor item while the row is selected. Row 0xFFFE is greyed out
/// and unselectable unless `Gp_HasMappedItem` answers non-zero, and opens its
/// own panel; row 0xFFFC is greyed out while the scan holds item 0x8F. Any
/// other row is an item with its price, greyed out when `func_800B7420`
/// refuses it; confirm opens the buy panel and button 0x10 the item's detail
/// panel.
static void Shop_ItemRow(UiList* prompt, UiObject* obj)
{
    TextDrawReq         req;
    u8                  buf[0x20];
    _ShopItemListWork*  work;
    InventoryItemRange* scan;
    s32                 y;
    s32                 scaled;
    UiObject*           child;
    UiObject*           child2;
    s32                 blocked;
    s32                 status;
    s32                 itemId;
    s32                 price;

    work    = obj->owner->work;
    blocked = 0;
    itemId  = work->rowIds[prompt->currentItemIndex];
    /* &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems hoisted into a saved register here, as the original does,
       instead of being rematerialised at the Gp_SumScanQty call. */
    scan = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    if (prompt->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        Shop_Data_801819EC = itemId;
    }

    if (itemId == 0xFFFE) {
        status = obj->panel.control.word;
        if (((status >> 16) == 1) || (status == 1)) {
            if (prompt->selectedItemIndex == prompt->currentItemIndex) {
                Ui_SetHolderParam(Shop_Data_80181A20, 0, 0);
            }
        }
        if (Gp_HasMappedItem() == 0) {
            prompt->colorRgb        = Ui_LookupTable(obj, 2);
            prompt->rowInputEnabled = USER_INTERFACE_LIST_ROW_INACTIVE;
        }
        req.x          = obj->panel.contentOriginX.unsignedValue + prompt->rowTextX.signedValue;
        y              = obj->panel.contentOriginY.unsignedValue - 4;
        req.y          = prompt->rowTextY.signedValue + y;
        req.otIndex    = obj->panel.otIndex.signedValue + 1;
        req.colorRgb   = prompt->colorRgb;
        req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        req.alignment  = TEXT_ALIGNMENT_LEFT;
        req.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&req, Shop_Data_80181A0C);
        if (prompt->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
            Ui_SpawnFromDesc(&Shop_Data_80181BD8, 0, 1, 1, obj);
            obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        }
        return;
    }

    if (itemId == 0xFFFC) {
        status = obj->panel.control.word;
        if (((status >> 16) == 1) || (status == 1)) {
            if (prompt->selectedItemIndex == prompt->currentItemIndex) {
                Ui_SetHolderParam(Gp_StrEmpty, 0, 0);
            }
        }
        if (Gp_SumScanQty(scan, 0x8F) != 0) {
            blocked          = 1;
            prompt->colorRgb = Ui_LookupTable(obj, 2);
        }
        textDrawUiLine(obj, prompt->rowTextX.signedValue, prompt->rowTextY.signedValue, Shop_Data_80181A1C, prompt->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
        if (prompt->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE && blocked == 0 && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
            child = Ui_SpawnFromDesc(&Shop_Data_80181B84, itemId, 1, 1, obj);
            if (child != NULL) {
                uiPositionRowDialog(&(child)->panel, prompt, &(obj)->panel);
                obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            }
        }
        return;
    }

    price = Gp_ItemDescs[itemId].price;
    if (func_800B7420(itemId) != 0) {
        blocked          = 1;
        prompt->colorRgb = Ui_LookupTable(obj, 2);
    }
    if (prompt->actionResult != USER_INTERFACE_LIST_ACTION_SKIP_ROW) {
        status = obj->panel.control.word;
        if (((status >> 16) == 1) || (status == 1)) {
            if (prompt->selectedItemIndex == prompt->currentItemIndex) {
                Gp_SetHolderItemText(itemId);
                Gp_SetPreviewItem(itemId, 0);
            }
        }
    }
    if (prompt->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (blocked == 0 && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            child2 = Ui_SpawnFromDesc(&Shop_Data_80181B84, itemId, 1, 1, obj);
            if (child2 != NULL) {
                sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
                uiPositionRowDialog(&(child2)->panel, prompt, &(obj)->panel);
                obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_TRIANGLE) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            Ui_SpawnFromDesc(&D_8010EFA0, itemId, 1, 1, obj);
            obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        }
    }
    Gp_DrawItemLabel(obj, prompt->rowTextX.signedValue, prompt->rowTextY.signedValue, itemId, prompt->colorRgb, 0);
    if ((u32)(itemId - 0xA0) < 0x20) {
        /* Dead: emits the scaled index before the table base so the
           `addu` is index-first, matching the original. */
        scaled = itemId * 4;
        Gp_DrawQty(obj, prompt->rowTextX.signedValue, prompt->rowTextY.signedValue, gpItemStock(itemId)->packQty, prompt->colorRgb);
    }
    textItoaUnsigned(buf, price);
    textDrawUiLine(obj, -prompt->rowTextX.signedValue, prompt->rowTextY.signedValue, buf, prompt->colorRgb, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
}

/// Adds an item id to the room's shop list, keeping one entry per item kind:
/// ids 0xF..0x32 are three consecutive levels of the same kind, so an entry of
/// the same kind is overwritten only by a higher level. In mode 0x10 the ids
/// 0x9D..0x9F, 0x8A and 0x65 are never added.
///
/// `list` is the panel's list control, whose `itemCount` counts the ids; the
/// ids themselves go to the `_ShopItemListWork` of the task owning `obj`.
static void Shop_AddItem(UiList* list, UiObject* obj, s32 item)
{
    Task*              task = obj->owner;
    s32                mode = task->spawnArg1.value;
    _ShopItemListWork* work = task->work;
    s32                i;

    for (i = 0; i < list->itemCount; i++) {
        s32 cur = work->rowIds[i];
        s32 q;

        if (cur == item) {
            return;
        }
        if (((mode & 0xFFFF) == 0x10) &&
            (((u32)(item - 0x9D) < 3U) || (item == 0x8A) || (item == 0x65))) {
            return;
        }
        if (((u32)(item - 0xF) < 0x24U) && ((u16)(cur - 0xF) < 0x24U)) {
            q = (item - 0xF) / 3;
            if ((q == (cur - 0xF) / 3) && (((item - 0xF) % 3 + 1) > ((cur - 0xF) % 3 + 1))) {
                work->rowIds[i] = item;
                return;
            }
        }
    }

    Gp_SetItemSeenBit(item, 1);
    work->rowIds[list->itemCount] = item;
    list->itemCount++;
}

/// Fills the `_ShopItemListWork` of the task owning `obj` with the ids the
/// shop currently offers, counting them into `list`, the list control of that
/// work block. It then sorts them by `Gp_ItemSortKey`, caps the visible row
/// count at 9 and clears the cursor item.
///
/// The upper halfword of the owning task's `spawnArg1` is the mode, which picks
/// the fixed id list (`Shop_SelectStock`) and, in game mode
/// 0, which items of each unlocked `ShopTier` row are added: mode 0 ids 0x80-0x9F
/// and 9, 0xA, 0xC, 0x42-0x46; mode 1 ids 0xA0-0xBF; mode 2 ids 0x60-0x7F and
/// 0xD; mode 3 ids 1-0x5F other than those. Mode 3 also adds, for each of the
/// twelve two-bit levels in `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.shopStock`, the id of that level
/// (the first slot needs level 2). With `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene` 1 every row
/// and level is unlocked first.
static void Shop_BuildItemList(UiList* list, UiObject* obj)
{
    _ShopItemListWork* work;
    u16*               ids;
    s32                mode;
    s32                tier;
    s32                slot;
    s32                level;
    s32                id;
    s32                item;
    s32                unlocked;
    s32                i;
    s32                j;
    s32                k;
    s32                key;
    s32                otherKey;
    u16                tmp;
    u8                 count;

    mode = obj->owner->spawnArg1.value;
    ids  = Shop_SelectStock(mode);

    list->itemCount = 0;
    while (*ids != 0xFFFF) {
        Shop_AddItem(list, obj, *ids);
        ids++;
    }

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene == 1) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.shopTiers = SHOP_TIER_ALL_MASK;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.shopStock = -1;
    }

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode == 0) {
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.shopTiers != 0) {
            for (tier = 0; tier < SHOP_TIER_COUNT; tier++) {
                unlocked = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.shopTiers & (1 << tier);
                if (unlocked != 0) {
                    for (j = 0; j < ARRAY_SIZE(Shop_Data_80181950[tier].items); j++) {
                        item = Shop_Data_80181950[tier].items[j];
                        switch (mode >> 16) {
                            case 0:
                                if (((u32)(item - 0x80) < 0x20U) || (item == 0xC) || (item == 9) ||
                                    (item == 0xA) || (item == 0x46) || (item == 0x45) ||
                                    (item == 0x42) || (item == 0x43) || (item == 0x44)) {
                                    Shop_AddItem(list, obj, item);
                                }
                                break;
                            case 1:
                                if ((u32)(item - 0xA0) < 0x20U) {
                                    Shop_AddItem(list, obj, item);
                                }
                                break;
                            case 2:
                                if (((u32)(item - 0x60) < 0x20U) || (item == 0xD)) {
                                    Shop_AddItem(list, obj, item);
                                }
                                break;
                            case 3:
                                if (((u32)(item - 1) < 0x5FU) && (item != 0xD) && (item != 0xC) &&
                                    (item != 9) && (item != 0xA) && (item != 0x46) &&
                                    (item != 0x45) && (item != 0x42) && (item != 0x43) &&
                                    (item != 0x44)) {
                                    Shop_AddItem(list, obj, item);
                                }
                                break;
                        }
                    }
                }
            }
        }

        if ((mode >> 16) == 3) {
            for (slot = 0; slot < 0xC; slot++) {
                level = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.shopStock >> (slot * 2)) & 3;
                if (slot == 0 ? level >= 2 : level > 0) {
                    /* The assignment keeps `+ 0xE` on the level instead of
                       letting GCC reassociate it onto the row base. */
                    Shop_AddItem(list, obj, slot * 3 + (id = level + 0xE));
                }
            }
        }
    }

    work = obj->owner->work;
    for (i = 0; i < list->itemCount - 1; i++) {
        key = Gp_ItemSortKey(work->rowIds[i]);
        for (k = i + 1; k < list->itemCount; k++) {
            otherKey = Gp_ItemSortKey(work->rowIds[k]);
            if (otherKey < key) {
                tmp             = work->rowIds[i];
                key             = otherKey;
                work->rowIds[i] = work->rowIds[k];
                work->rowIds[k] = tmp;
            }
        }
    }

    count                               = list->itemCount;
    list->visibleRowCount.unsignedValue = count;
    if ((s8)count >= 0xA) {
        list->visibleRowCount.unsignedValue = 9;
    }
    Shop_Data_801819EC = -1;
}

static const u8 Shop_Data_8017D6D0[] = "Select";

static const u8 Shop_Data_8017D6D8[] = "BP";

static const u8 Shop_Data_8017D6DC[] = "List";

static const u8 Shop_Data_8017D6E4[] = "TOTAL";

static const u8 Shop_Data_8017D6EC[] = "Notice";

static const char Shop_Data_8017D6F4[8] = SHOP_CHARGE_TITLE_BYTES;

/// The shop's "Select" panel. On its first frame it allocates the
/// `_ShopItemListWork` work block, fills it through
/// `Shop_BuildItemList` and opens the panel
/// `Shop_Data_80181BF4` beside it. Every frame it draws the list and the
/// "BP" caption; menu reports -1 and cancel 6 to the parent. A child that
/// reports 6 is torn down and the list takes input again; one that reports -1
/// passes it up.
static void Shop_ItemListTask(Task* task)
{
    TextDrawReq        req;
    UiObject*          obj;
    _ShopItemListWork* work;
    Task*              head;
    Task*              child;
    Task*              next;
    UiObject*          childObj;
    void*              mem;
    s32                code;
    s32                x;
    s32                y;

    obj         = task->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&(obj)->panel, (const char*)Shop_Data_8017D6D0);
    if (task->state == 0) {
        mem = memCalloc(sizeof(_ShopItemListWork), 0);
        if (mem != NULL) {
            work                      = mem;
            task->work                = work;
            work->list.rowCallbacks   = Shop_Data_80181AD8;
            work->list.wrapNavigation = 0;
            work->list.rowHeight      = 0xF;
            Shop_BuildItemList(&work->list, obj);
            Ui_LayoutListPanel(&work->list, &(obj)->panel);
            work->list.flags = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
            Ui_SetListScrollFlag(&work->list, 1);
            obj->panel.bounds.unsignedRect.h += 8;
            work->list.topInset               = 8;
            Ui_SpawnFromDesc(&Shop_Data_80181BF4, 0, 0, 0, obj);
            task->state += 1;
        }
    }
    work = task->work;
    Ui_UpdateListNoAnim(&work->list, obj);
    uiDrawHorizontalSeparator(&(obj)->panel, obj->panel.contentLeft.signedValue, obj->panel.contentRight.signedValue, obj->panel.contentTop.signedValue + 6);

    x              = obj->panel.contentOriginX.unsignedValue - 2;
    req.x          = obj->panel.contentRight.unsignedValue + x;
    y              = obj->panel.contentOriginY.unsignedValue + 2;
    req.y          = obj->panel.contentTop.unsignedValue + y;
    req.otIndex    = obj->panel.otIndex.signedValue + 1;
    req.colorRgb   = 0x606060;
    req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req.alignment  = TEXT_ALIGNMENT_RIGHT;
    req.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req, Shop_Data_8017D6D8);

    if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            obj->result = USER_INTERFACE_RESULT_CANCEL;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
            obj->result = USER_INTERFACE_RESULT_CONFIRM;
        }
    }

    head = task->firstChild;
    if (head != NULL) {
        child = head;
        do {
            childObj = child->spawnArg2.pointer;
            code     = childObj->result;
            next     = child->nextSibling;
            if (code != USER_INTERFACE_RESULT_CANCEL) {
                if (code == USER_INTERFACE_RESULT_CONFIRM) {
                    uiStartTreeClosing(childObj, childObj->owner);
                    obj->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
                }
            } else {
                obj->result = code;
            }
            child = next;
        } while (child != task->firstChild);
    }
}

/// Row handler of the shop's mode menu. The last row is the exit, which
/// reports 6 on confirm. Any other row stores its index as the owning task's
/// mode (the upper halfword of `spawnArg1`) and draws that mode's label; the
/// row is greyed out and unselectable when the mode's item-id list is empty,
/// and confirm opens the shop list panel with the mode.
static void Shop_CategoryRow(UiList* prompt, UiObject* obj)
{
    u8* text;
    s32 status;
    s32 one;
    s32 one2;

    if ((prompt->itemCount - 1) == prompt->currentItemIndex) {
        one = 1;
        textDrawUiLine(obj, prompt->rowTextX.signedValue, prompt->rowTextY.signedValue, Shop_Data_80181A04, prompt->colorRgb, one, TEXT_ALIGNMENT_LEFT);
        if (prompt->rowInputEnabled == one && padCheckButtons(0, one, Pad_MaskConfirm) != 0) {
            obj->result = USER_INTERFACE_RESULT_CONFIRM;
        }
        return;
    }

    text                        = Shop_Data_80181A5C;
    obj->owner->spawnArg1.value = (u16)obj->owner->spawnArg1.value;
    switch (prompt->currentItemIndex) {
        case 0:
            break;
        case 1:
            text                         = Shop_Data_80181A64;
            obj->owner->spawnArg1.value |= 0x10000;
            break;
        case 2:
            text                         = Shop_Data_80181A70;
            obj->owner->spawnArg1.value |= 0x20000;
            break;
        case 3:
            text                         = Shop_Data_80181A78;
            obj->owner->spawnArg1.value |= 0x30000;
            break;
    }

    if (*Shop_SelectStock(obj->owner->spawnArg1.value) == 0xFFFF) {
        prompt->colorRgb        = Ui_LookupTable(obj, 2);
        prompt->rowInputEnabled = USER_INTERFACE_LIST_ROW_INACTIVE;
    }

    one2 = 1;
    textDrawUiLine(obj, prompt->rowTextX.signedValue, prompt->rowTextY.signedValue, text, prompt->colorRgb, one2, TEXT_ALIGNMENT_LEFT);

    status = obj->panel.control.word;
    if (((status >> 16) == one2) || (status == one2)) {
        if (prompt->selectedItemIndex == prompt->currentItemIndex) {
            Ui_SetHolderParam(Gp_StrEmpty, 0, 0);
        }
    }

    if (prompt->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
        sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
        Ui_SpawnFromDesc(&Shop_Data_80181B4C, obj->owner->spawnArg1, 1, 1, obj);
        obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
    }
}

/// The shop's "List" panel, whose rows are the modes. Its first frame clears the item previews,
/// opens the list-row panel and the preview panel, and lays out its five-row
/// list. Cancel or menu reports -1. A child reporting 6 is torn down; one
/// reporting -1 releases `Wip_UiHolder` and passes the code up.
static void Shop_CategoryListTask(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    Task*     head;
    UiObject* childObj;
    s32       code;

    obj         = task->spawnArg2.pointer;
    list        = &Shop_Data_80181AE0;
    obj->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&(obj)->panel, (const char*)Shop_Data_8017D6DC);
    if (task->state == 0) {
        Gp_ClearPreviewItems();
        D_80067634 = NULL;
        Ui_SpawnFromDesc(&Shop_Data_80181B68, task->spawnArg1, 0, 1, obj);
        Ui_SpawnFromDesc(&D_8010D80C, 0, 0, 0, obj);
        list->itemCount                     = 5;
        list->visibleRowCount.unsignedValue = 5;
        Ui_LayoutListPanel(list, &(obj)->panel);
        list->flags = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        Ui_SetListScrollFlag(list, 1);
        task->state += 1;
    }
    Ui_UpdateListNoAnim(list, obj);
    if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel | Pad_MaskMenu) != 0) {
        obj->result = USER_INTERFACE_RESULT_CANCEL;
    }

    head = task->firstChild;
    if (head != NULL) {
        child = head;
        do {
            childObj = child->spawnArg2.pointer;
            code     = childObj->result;
            next     = child->nextSibling;
            if (code != USER_INTERFACE_RESULT_CANCEL) {
                if (code == USER_INTERFACE_RESULT_CONFIRM) {
                    uiStartTreeClosing(childObj, childObj->owner);
                    obj->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
                }
            } else {
                Wip_UiHolder = NULL;
                obj->result  = code;
            }
            child = next;
        } while (child != task->firstChild);
    }
}

/// Balance panel: the "BP" caption with the player's BP, and the "TOTAL"
/// caption with the carried item count over the inventory's row capacity.
static void Shop_BalanceTask(Task* task)
{
    u8                  digits[0x20];
    u8                  total[0x20];
    TextDrawReq         req0;
    TextDrawReq         req1;
    UiObject*           obj;
    PlayerStatus*       cfg;
    InventoryItemRange* scan;
    u8*                 p;
    s32                 x;
    s32                 y;
    s32                 y2;
    s32                 col;
    s32                 capacity;
    s32                 count;

    obj = task->spawnArg2.pointer;
    cfg = &gPlayerStatus;
    x   = obj->panel.contentLeft.signedValue + 2;
    col = obj->panel.contentRight.signedValue - 2;
    y   = obj->panel.contentTop.signedValue;

    req0.x          = obj->panel.contentOriginX.unsignedValue + x;
    req0.y          = obj->panel.contentOriginY.unsignedValue + y + 9;
    req0.otIndex    = obj->panel.otIndex.signedValue + 1;
    req0.colorRgb   = 0x606060;
    req0.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req0.alignment  = TEXT_ALIGNMENT_LEFT;
    req0.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req0, Shop_Data_8017D6D8);

    textItoaUnsigned(digits, cfg->bp);
    textDrawUiLine(obj, col, y + 0x19, digits, 0x606060, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);

    y2              = y + 0x28;
    req1.x          = obj->panel.contentOriginX.unsignedValue + x;
    req1.y          = obj->panel.contentOriginY.unsignedValue + (y2 - 6);
    req1.otIndex    = obj->panel.otIndex.signedValue + 1;
    req1.colorRgb   = 0x606060;
    req1.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req1.alignment  = TEXT_ALIGNMENT_LEFT;
    req1.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req1, Shop_Data_8017D6E4);

    p        = total;
    scan     = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    count    = Gp_CountScanItems(scan);
    capacity = scan->rowCount;
    textItoaUnsigned(p, count);
    while (*(const s8*)p != 0) {
        p++;
    }
    *p = '/';
    textItoaUnsigned(p + 1, capacity);
    textDrawUiLine(obj, col, y2 + 0xA, total, 0x606060, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
}

/// Row handler of the buy prompt. On confirm it checks the price against the
/// player's BP (notice 0 when short) and the inventory (notice 2 for a
/// stackable item already held, 1 otherwise when it cannot be added). When the
/// owning task's parent runs in mode 1 it opens the quantity picker; otherwise
/// it takes the price, gives one of the item and reports 6.
static void Shop_BuyRow(UiList* prompt, UiObject* obj)
{
    TextDrawReq         req;
    UiObject*           child;
    PlayerStatus*       cfg;
    InventoryItemRange* scan;
    s32                 itemId;
    s32                 mode;
    s32                 price;

    itemId = obj->owner->spawnArg1.value;

    req.x          = obj->panel.contentOriginX.unsignedValue + prompt->rowTextX.unsignedValue;
    req.y          = obj->panel.contentOriginY.unsignedValue + prompt->rowTextY.unsignedValue;
    req.otIndex    = obj->panel.otIndex.signedValue + 1;
    req.colorRgb   = prompt->colorRgb;
    req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req.alignment  = TEXT_ALIGNMENT_LEFT;
    req.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req, Shop_Data_801819F0);

    mode = prompt->rowInputEnabled;
    if (mode == 1 && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
        cfg   = &gPlayerStatus;
        price = Gp_ItemDescs[itemId].price;
        scan  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
        sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
        if (cfg->bp >= price) {
            if (Gp_CanAddItem(scan, itemId) == 0) {
                if ((u32)(itemId - 0xA0) < 0x20U && Gp_SumScanQty(scan, itemId) != 0) {
                    Ui_SpawnFromDesc(&Shop_Data_80181BA0, 2, 1, 1, obj);
                } else {
                    Ui_SpawnFromDesc(&Shop_Data_80181BA0, 1, 1, 1, obj);
                }
                obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            } else if ((obj->owner->parent->spawnArg1.value >> 16) == mode) {
                child = Ui_SpawnFromDesc(&Shop_Data_80181C10, itemId, 1, 1, obj);
                if (child != NULL) {
                    uiPositionRowDialog(&(child)->panel, prompt, &(obj)->panel);
                    obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                }
            } else {
                cfg->bp -= price;
                Gp_GiveItem(scan, itemId, -1);
                obj->result = USER_INTERFACE_RESULT_CONFIRM;
            }
        } else {
            Ui_SpawnFromDesc(&Shop_Data_80181BA0, 0, 1, 1, obj);
            obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        }
    }
}

/// Notice panel: shows one of three messages picked by `spawnArg1`, sized to
/// the text. Menu reports -1; confirm, cancel or 0xBC frames elapsing tell the
/// parent panel to close with 6.
static void Shop_NoticeTask(Task* task)
{
    UiObject* obj;
    u8*       text;
    s32       kind;

    kind = task->spawnArg1.value;
    obj  = task->spawnArg2.pointer;
    switch (kind) {
        case 1:
            text = Shop_Data_80181A94;
            break;
        case 2:
            text = Shop_Data_80181AA4;
            break;
        default:
            text = Shop_Data_80181A80;
            break;
    }

    uiDrawPanelLabel(&(obj)->panel, (const char*)Shop_Data_8017D6EC);
    obj->result = USER_INTERFACE_RESULT_NONE;
    if (task->state == 0) {
        Ui_SizeFromTextPlain(&(obj)->panel, text);
        task->killCountdown = 0xBC;
        task->state        += 1;
    }
    textDrawUiLines(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0xF, text, 0x606060, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    task->killCountdown -= gDisplayState.frameTicks;
    if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            obj->result = USER_INTERFACE_RESULT_CANCEL;
            return;
        }
        if (task->killCountdown <= 0 || padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
            ((UiObject*)task->parent->spawnArg2.pointer)->result = USER_INTERFACE_RESULT_CONFIRM;
            task->killCountdown                                  = 0x7FFF;
        }
    }
}

/// The charge panel: steps through the built-in supplies of carried weapons,
/// refilling each supply's load to its capacity and animating a bar from the
/// old value up to the new one for at most 0xBC frames. Confirm or cancel (or
/// the timer running out) moves to the next supply; running out of supplies
/// reports 6 to the parent.
static void Shop_ChargeTask(Task* task)
{
    UiObject*              obj;
    EquipmentWeaponSupply* supply;
    EquipmentWeaponLoad*   slot;
    s32                    slotId;
    s32                    itemId;
    s32                    weaponItemId;
    s32                    supplyItemId;
    s32                    qty;
    s32                    y;
    s32                    h;
    s32                    status;
    s16                    countdown;

    obj         = task->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&(obj)->panel, Shop_Data_8017D6F4);

    if (task->state == 0) {
        task->spawnArg1.value = 0;
        task->state           = task->state + 1;
    }
    if (task->state == 1) {
        slotId                = Gp_NextMappedSlot(task->spawnArg1.value);
        task->spawnArg1.value = slotId;
        if (slotId < 0) {
            obj->result = USER_INTERFACE_RESULT_CONFIRM;
        } else {
            supply             = Gp_GetItemMap(slotId);
            Shop_Data_8018762C = supply;
            itemId             = supply->weaponItemId;
            slot               = Gp_GetItemSlot(itemId);
            if (Shop_Data_8018762C->supplyLoad == EQUIPMENT_WEAPON_SUPPLY_PRIMARY) {
                Shop_Data_80187628 = slot->primaryQty;
                slot->primaryQty   = Gp_GetRelatedQty(itemId, EQUIPMENT_WEAPON_SUPPLY_PRIMARY);
            } else {
                Shop_Data_80187628 = slot->secondaryQty;
                slot->secondaryQty = Gp_GetRelatedQty(itemId, EQUIPMENT_WEAPON_SUPPLY_SECONDARY);
            }
            task->killCountdown  = 0xBC;
            Shop_Data_80187628 <<= 8;
            task->state          = task->state + 1;
        }
    }

    weaponItemId = Shop_Data_8018762C->weaponItemId;
    supplyItemId = Shop_Data_8018762C->supplyItemId;
    if (Shop_Data_8018762C->supplyLoad == EQUIPMENT_WEAPON_SUPPLY_PRIMARY) {
        qty = Gp_GetRelatedQty(weaponItemId, EQUIPMENT_WEAPON_SUPPLY_PRIMARY);
    } else {
        qty = Gp_GetRelatedQty(weaponItemId, EQUIPMENT_WEAPON_SUPPLY_SECONDARY);
    }
    qty               <<= 8;
    Shop_Data_80187628 += 0x40;
    if (qty < Shop_Data_80187628) {
        Shop_Data_80187628 = qty;
    }

    y = obj->panel.contentTop.signedValue;
    Gp_DrawItemLabel(obj, obj->panel.contentLeft.signedValue + 2, y + 0xF, weaponItemId, 0x606060, 0);
    uiDrawHorizontalSeparator(&(obj)->panel, obj->panel.contentLeft.signedValue, obj->panel.contentRight.signedValue, y + 0x12);
    Gp_DrawItemLabel(obj, obj->panel.contentLeft.signedValue + 2, y + 0x23, supplyItemId, 0x606060, 0);
    Gp_DrawQty(obj, obj->panel.contentLeft.signedValue + 2, y + 0x23, Shop_Data_80187628 >> 8, 0x606060);
    h = obj->panel.contentBottom.signedValue;
    func_800C0E20(&(obj)->panel, obj->panel.contentLeft.signedValue + 2, obj->panel.contentRight.signedValue - 2, h - 6, qty,
                  Shop_Data_80187628, 0x1741F);

    if (task->state == 2) {
        countdown           = task->killCountdown - 1;
        task->killCountdown = countdown;
        status              = obj->panel.control.word;
        if (status == 1 && (countdown <= 0 || padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel | Pad_MaskConfirm) != 0)) {
            task->state           = status;
            task->spawnArg1.value = task->spawnArg1.value + 1;
        }
    }
}

static inline s32 Shop_AddItemCount(s32 item, s32 count)
{
    s32                 i;
    s32                 n;
    InventoryItemRow*   rec;
    InventoryItemRange* scan;

    if ((u32)(item - 0xA0) < 0x20U) {
        count += Gp_ScanStackQty(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, item);
    } else {
        scan = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
        rec  = Gp_GetItemTable(scan) + scan->firstRow;
        n    = scan->rowCount;
        for (i = 0; i < n; i++) {
            if (rec[i].itemId == item) {
                count++;
            }
        }
    }
    return count;
}

/// Draws the preview of the item the shop list's cursor rests on and, for an
/// item id below 0x100, the "Amount" caption with how many of it the player
/// already holds. Stackable items (0xA0..0xBF) ask the scan for their stack
/// quantity; everything else is counted by walking the item table.
static void Shop_PreviewTask(Task* task)
{
    u8          buf[0x10];
    TextDrawReq req;
    UiObject*   obj;
    s32         item;
    s32         y;
    s32         ry;
    s32         count;

    item         = Shop_Data_801819EC;
    obj          = task->spawnArg2.pointer;
    task->status = 0;
    if ((CdCmd_IsIdle() & 0xFFFF) && Shop_Data_801819EC == Gp_GetPreviewItem()) {
        func_800C7AE8(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 2, 0x20);
    } else {
        func_800C7AE8(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 2, 0x120);
    }
    y = obj->panel.contentTop.signedValue + 0x50;
    if (item < 0x100) {
        req.x          = obj->panel.contentLeft.signedValue + (obj->panel.contentOriginX.unsignedValue + 2);
        ry             = obj->panel.contentOriginY.unsignedValue - 6;
        req.y          = ry + y;
        req.otIndex    = obj->panel.otIndex.signedValue + 1;
        req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
        req.colorRgb   = 0x606060;
        req.alignment  = TEXT_ALIGNMENT_LEFT;
        req.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&req, Shop_Data_80181AC4);
        count = 0;
        count = Shop_AddItemCount(item, count);
        textDrawUiLine(obj, obj->panel.contentRight.signedValue - 2, y + 0xA, textItoaSigned(buf, count), 0x606060, TEXT_DRAW_TRANSLUCENT_OUTLINED,
                       TEXT_ALIGNMENT_RIGHT);
    }
}

/// Quantity picker of the buy prompt. Up and down step the count between 1 and
/// the most the player can take: for a stackable item, what its stock ceiling
/// still allows in steps of its per-buy amount; otherwise the free inventory
/// rows; in both cases no more than the BP affords. It shows the unit and
/// total price. Confirm takes the total and gives the items; confirm or
/// cancel tells the parent panel to close with 6.
static void Shop_QuantityTask(Task* task)
{
    u8           buf[0x20];
    TextDrawReq  req;
    UiObject*    obj;
    UiObject*    parentObj;
    s32          itemId;
    s32          price;
    s32          maxQty;
    s32          afford;
    s32          held;
    register s32 maxHeld asm("v0");
    s32          count;
    s32          left;
    s32          top;
    s32          x;
    s32          y;
    s32          i;

    itemId = task->spawnArg1.value;
    obj    = task->spawnArg2.pointer;
    maxQty = 1;
    price  = Gp_ItemDescs[itemId].price;

    if (task->state == 0) {
        task->extraState.value = 1;
        uiSetPanelContentSize(&(obj)->panel, 0, uiGetTextRowsHeight(3) - 3);
        task->state = task->state + 1;
    }

    if ((u32)(itemId - 0xA0) < 0x20) {
        InventoryConsumableStack* stock = gpItemStock(itemId);

        if (stock->packQty != 0) {
            held    = Gp_ScanStackQty(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, itemId);
            maxHeld = stock->maxHeld;
            maxQty  = maxHeld - held;
            if (maxQty <= 0) {
                maxQty = 1;
            } else {
                maxQty = (maxQty - 1) / stock->packQty;
                maxQty = maxQty + 1;
            }
        }
    } else {
        maxQty = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems.rowCount - Gp_CountScanItems(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems);
    }

    afford = gPlayerStatus.bp / price;
    if (afford < maxQty) {
        maxQty = afford;
    }

    left = obj->panel.contentLeft.signedValue;
    x    = left + 2;
    top  = obj->panel.contentTop.signedValue;
    y    = top + 0xF;
    Gp_DrawItemLabel(obj, x, y, itemId, 0x606060, 0);
    if ((u32)(itemId - 0xA0) < 0x20) {
        InventoryConsumableStack* stock = gpItemStock(itemId);

        Gp_DrawQty(obj, x, y, stock->packQty, 0x606060);
    }

    count = task->extraState.value;
    textDrawUiLine(obj, left + 0x98, y, Shop_Data_80181AD0, 0x606060, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
    textDrawUiLine(obj, -x, y, textItoaSigned(buf, count), 0x606060, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
    uiDrawHorizontalSeparator(&(obj)->panel, left, -x + 2, top + 0x12);

    req.x          = obj->panel.contentOriginX.unsignedValue - x;
    y              = top + 0x1A;
    req.y          = obj->panel.contentOriginY.unsignedValue + y;
    req.otIndex    = obj->panel.otIndex.signedValue + 1;
    req.colorRgb   = 0x606060;
    req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req.alignment  = TEXT_ALIGNMENT_RIGHT;
    req.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req, Shop_Data_8017D6D8);

    textDrawUiLine(obj, -x, top + 0x2B, textItoaSigned(buf, count * price), 0x606060, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);

    if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        parentObj = task->parent->spawnArg2.pointer;
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_UP | PAD_BUTTON_RIGHT) != 0) {
            if (task->extraState.value < maxQty) {
                task->extraState.value = task->extraState.value + 1;
                sndEvtRequestScriptStart(SOUND_SYSTEM_CURSOR, 0, 0);
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_DOWN | PAD_BUTTON_LEFT) != 0) {
            if (task->extraState.value >= 2) {
                task->extraState.value = task->extraState.value - 1;
                sndEvtRequestScriptStart(SOUND_SYSTEM_CURSOR, 0, 0);
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            gPlayerStatus.bp -= price * task->extraState.value;
            for (i = 0; i < task->extraState.value; i++) {
                Gp_GiveItem(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, itemId, -1);
            }
            sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
            parentObj->result = USER_INTERFACE_RESULT_CONFIRM;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
            parentObj->result = USER_INTERFACE_RESULT_CONFIRM;
        }
    }
}

/// Row handler that draws a single message and reports 6 on confirm.
static void Shop_MessageRow(UiList* prompt, UiObject* obj)
{
    TextDrawReq req;

    req.x          = obj->panel.contentOriginX.unsignedValue + prompt->rowTextX.unsignedValue;
    req.y          = obj->panel.contentOriginY.unsignedValue + prompt->rowTextY.unsignedValue;
    req.otIndex    = obj->panel.otIndex.signedValue + 1;
    req.colorRgb   = prompt->colorRgb;
    req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req.alignment  = TEXT_ALIGNMENT_LEFT;
    req.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req, Shop_Data_80181A04);

    if (prompt->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
        sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
        obj->result = USER_INTERFACE_RESULT_CONFIRM;
    }
}

/// A list panel over `Shop_Data_80181B0C`. Cancel reports 6 and
/// menu -1; a child reporting 6 is torn down, one reporting -1 passes it up.
static void Shop_BuyPromptTask(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    UiObject* childObj;
    s16       code;

    list        = &Shop_Data_80181B0C;
    obj         = task->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    if (task->state == 0) {
        Ui_LayoutListPanel(list, &(obj)->panel);
        Ui_SetListScrollFlag(list, 1);
        task->state += 1;
    }
    Ui_UpdateListNoAnim(list, obj);
    if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
            obj->result = USER_INTERFACE_RESULT_CONFIRM;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            obj->result = USER_INTERFACE_RESULT_CANCEL;
        }
    }

    child = task->firstChild;
    if (child != NULL) {
        childObj = child->spawnArg2.pointer;
        code     = childObj->result;
        if (code != USER_INTERFACE_RESULT_CANCEL) {
            if (code == USER_INTERFACE_RESULT_CONFIRM) {
                uiStartTreeClosing(childObj, childObj->owner);
                obj->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
            }
        } else {
            obj->result = USER_INTERFACE_RESULT_CANCEL;
        }
    }
}

/// Opens the panel `Shop_Data_80181B30` with the task's
/// `spawnArg1` as its parameter, setting frame timing 0 and the session's UI
/// flag while it is open; once the panel reports cancel or confirm it is torn down, and
/// ten frames later frame timing 1 and the flag are restored and the task
/// kills itself.
static void Shop_SessionTask(Task* task)
{
    UiObject* obj;

    if (task->state == 0) {
        Stage_InitPrimBufOnce();
        obj = Ui_SpawnFromDesc(&Shop_Data_80181B30, task->spawnArg1, 1, 1, NULL);
        if (obj == NULL) {
            return;
        }
        displaySetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
        gGameSession->uiOpen    = 1;
        task->spawnArg2.pointer = obj;
        task->state++;
    }

    if (task->state == 1) {
        obj = task->spawnArg2.pointer;
        if (obj->result == USER_INTERFACE_RESULT_CANCEL || obj->result == USER_INTERFACE_RESULT_CONFIRM) {
            uiStartTreeClosing(obj, obj->owner);
            task->killCountdown = 10;
            task->state         = 2;
        }
    }

    if (task->state == 2) {
        task->killCountdown--;
        if (task->killCountdown <= 0) {
            displaySetFrameTiming(DISPLAY_TIMING_TWO_VBLANKS);
            gGameSession->uiOpen = 0;
            taskKill(task);
            Stage_ReleasePrimBuf();
            Stage_SetEndingFlag();
        }
    }
}
