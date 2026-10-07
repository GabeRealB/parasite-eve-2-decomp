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

/// Stock-set selectors in the low halfword; categories occupy the high halfword.
///
/// Shelter's alternate set is selected by room variant or the soldier follow-up;
/// its event sets are selected while the sterilization-room event state is 2.
enum {
    SHOP_STOCK_MIST                    = 0x10,
    SHOP_STOCK_DRYFIELD                = 0x20,
    SHOP_STOCK_DRYFIELD_LATE           = 0x21,
    SHOP_STOCK_SHELTER                 = 0x30,
    SHOP_STOCK_SHELTER_ALTERNATE       = 0x31,
    SHOP_STOCK_SHELTER_EVENT           = 0x32,
    SHOP_STOCK_SHELTER_ALTERNATE_EVENT = 0x33,
    SHOP_STOCK_ARMORY                  = 0x40,
    SHOP_CATEGORY_WEAPONS              = 0,
    SHOP_CATEGORY_AMMUNITION           = 1,
    SHOP_CATEGORY_ARMOR                = 2,
    SHOP_CATEGORY_ITEMS                = 3,
    SHOP_CATEGORY_COUNT                = 4
};

/// Catalogue groups and individual accessories used to partition unlocked stock.
enum {
    SHOP_PE_ITEM_FIRST             = 0xF,
    SHOP_PE_ABILITY_COUNT          = 12,
    SHOP_PE_LEVEL_COUNT            = 3,
    SHOP_PE_ITEM_COUNT             = SHOP_PE_ABILITY_COUNT * SHOP_PE_LEVEL_COUNT,
    SHOP_PE_STOCK_BITS_PER_ABILITY = 2,
    SHOP_PE_STOCK_LEVEL_MASK       = 3,
    SHOP_PE_STOCK_ALL_LEVELS       = -1,
    SHOP_CARRIED_ITEM_ID_LIMIT     = 0x100,
    SHOP_GAME_MODE_NORMAL          = 0,
    SHOP_GAME_MODE_SCAVENGER       = 2,
    SHOP_ITEM_SMG_CLIP_HOLDER      = 9,
    SHOP_ITEM_RIFLE_CLIP_HOLDER    = 0xA,
    SHOP_ITEM_SNAIL_MAGAZINE       = 0xC,
    SHOP_ITEM_BELT_POUCH           = 0xD,
    SHOP_ITEM_HAMMER               = 0x42,
    SHOP_ITEM_PYKE                 = 0x43,
    SHOP_ITEM_JAVELIN              = 0x44,
    SHOP_ITEM_M203                 = 0x45,
    SHOP_ITEM_M9                   = 0x46,
    SHOP_ARMOR_ITEM_FIRST          = 0x60,
    SHOP_ARMOR_ITEM_COUNT          = 0x20,
    SHOP_ITEM_TACTICAL_VEST        = 0x65,
    SHOP_WEAPON_ITEM_COUNT         = 0x20,
    SHOP_ITEM_GRENADE_PISTOL       = 0x8A,
    SHOP_ITEM_MP5A5_FIRST          = 0x9D,
    SHOP_ITEM_MP5A5_COUNT          = 3
};

/// Panel initialization states; task storage keeps its existing integer widths.
enum { SHOP_PANEL_INITIAL = 0 };

/// Refill phases: initialize the scan, charge the next weapon, animate its supply.
enum { SHOP_REFILL_INITIAL     = 0,
       SHOP_REFILL_NEXT_SUPPLY = 1,
       SHOP_REFILL_ANIMATE     = 2 };

/// Shop-session phases, including the delay while its UI tree closes.
enum { SHOP_SESSION_INITIAL = 0,
       SHOP_SESSION_OPEN    = 1,
       SHOP_SESSION_CLOSING = 2 };

/// Purchase failure messages and display timing/units used by shop panels.
enum {
    SHOP_NOTICE_INSUFFICIENT_BP     = 0,
    SHOP_NOTICE_INVENTORY_FULL      = 1,
    SHOP_NOTICE_AMMUNITION_CAPACITY = 2,
    SHOP_NOTICE_DURATION_TICKS      = 188,
    SHOP_NOTICE_DISMISSED_COUNTDOWN = 0x7FFF,
    SHOP_REFILL_DURATION_FRAMES     = 188,
    SHOP_REFILL_FRACTION_BITS       = 8,
    SHOP_REFILL_STEP_FIXED          = 0x40, // One quarter of a supply unit per callback.
    SHOP_REFILL_METER_COLOR_RGB     = 0x1741F,
    SHOP_SESSION_CLOSE_FRAMES       = 10,
    SHOP_ITEM_LIST_VISIBLE_ROWS     = 9,
    SHOP_TEXT_COLOR_RGB             = 0x606060,
    SHOP_PREVIEW_NONE               = -1
};

/// Work block of the shop's item-list panel, parked in `Task::work`.
///
/// The panel's task allocates it on its first frame and the task's teardown
/// frees it. The list's row callback and the builders reach `rowIds` through
/// the owning task and index it by the list's item index; `list.itemCount` is
/// the number of ids in use. Current stock sets and unlock combinations
/// require at most 33 rows;
/// appends rely on that bound rather than checking the 64-row capacity.
typedef struct {
    UiList list;         // List control the panel is drawn from; its `itemCount` counts the `rowIds` in use
    u16    rowIds[0x40]; // Item or recharge-service row ids; at most 33 used by the current stock/unlock combinations
} _ShopItemListWork;
STATIC_ASSERT_SIZEOF(_ShopItemListWork, 0xA4);

static void Shop_ItemRow(UiList* prompt, UiObject* obj);

/// Borrows a consumable's pack quantity and per-stack capacity from the catalogue.
///
/// `consumableItemId` must be 0xA0..0xBF; no bounds check is performed.
/// Both quantities count item units. The read-only row remains valid while
/// the gameplay image is loaded; no inventory contents are read or changed.
static inline const InventoryConsumableStack* _inventoryGetConsumableStackInfo(s32 consumableItemId)
{
    return &Gp_StackLimits[consumableItemId - INVENTORY_CONSUMABLE_ITEM_FIRST];
}

/// Borrows the stock set for a packed stock-set/category selector.
///
/// The low halfword selects the vendor's stock profile; the unsigned high
/// halfword selects Weapons, Ammunition, Armor or Items (0..3). Save modes
/// below 2 use the normal/Bounty tables; modes 2 and above use the alternate
/// tables. Invalid categories return an empty list, including after the
/// Shelter switch fallthroughs. Rows end at `SHOP_ROW_END`; storage belongs
/// to this room's loaded image and is returned read-only.
static const u16* _shopSelectStockList(s32 stockSelector)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode < SHOP_GAME_MODE_SCAVENGER) {
        switch ((u16)stockSelector) {
            case SHOP_STOCK_SHELTER:
                switch ((u32)stockSelector >> 16) {
                    case SHOP_CATEGORY_WEAPONS:
                        return Shop_Data_801816D8;
                    case SHOP_CATEGORY_AMMUNITION:
                        return Shop_Data_801816F0;
                    case SHOP_CATEGORY_ARMOR:
                        return Shop_Data_80181704;
                    case SHOP_CATEGORY_ITEMS:
                        return Shop_Data_8018170C;
                }
            case SHOP_STOCK_SHELTER_ALTERNATE:
                switch ((u32)stockSelector >> 16) {
                    case SHOP_CATEGORY_WEAPONS:
                        return Shop_Data_80181720;
                    case SHOP_CATEGORY_AMMUNITION:
                        return Shop_Data_8018173C;
                    case SHOP_CATEGORY_ARMOR:
                        return Shop_Data_8018174C;
                    case SHOP_CATEGORY_ITEMS:
                        return Shop_Data_80181758;
                }
            case SHOP_STOCK_SHELTER_EVENT:
                switch ((u32)stockSelector >> 16) {
                    case SHOP_CATEGORY_WEAPONS:
                        return Shop_Data_80181770;
                    case SHOP_CATEGORY_AMMUNITION:
                        return Shop_Data_8018178C;
                    case SHOP_CATEGORY_ARMOR:
                        return Shop_Data_801817A0;
                    case SHOP_CATEGORY_ITEMS:
                        return Shop_Data_801817A8;
                }
            case SHOP_STOCK_SHELTER_ALTERNATE_EVENT:
                switch ((u32)stockSelector >> 16) {
                    case SHOP_CATEGORY_WEAPONS:
                        return Shop_Data_801817BC;
                    case SHOP_CATEGORY_AMMUNITION:
                        return Shop_Data_801817DC;
                    case SHOP_CATEGORY_ARMOR:
                        return Shop_Data_801817EC;
                    case SHOP_CATEGORY_ITEMS:
                        return Shop_Data_801817F8;
                }
            case SHOP_STOCK_DRYFIELD:
                switch ((u32)stockSelector >> 16) {
                    case SHOP_CATEGORY_WEAPONS:
                        return Shop_Data_80181620;
                    case SHOP_CATEGORY_AMMUNITION:
                        return Shop_Data_80181630;
                    case SHOP_CATEGORY_ARMOR:
                        return Shop_Data_80181640;
                    case SHOP_CATEGORY_ITEMS:
                        return Shop_Data_80181648;
                }
                break;
            case SHOP_STOCK_DRYFIELD_LATE:
                switch ((u32)stockSelector >> 16) {
                    case SHOP_CATEGORY_WEAPONS:
                        return Shop_Data_80181694;
                    case SHOP_CATEGORY_AMMUNITION:
                        return Shop_Data_801816AC;
                    case SHOP_CATEGORY_ARMOR:
                        return Shop_Data_801816C0;
                    case SHOP_CATEGORY_ITEMS:
                        return Shop_Data_801816C8;
                }
                break;
            case SHOP_STOCK_ARMORY:
                switch ((u32)stockSelector >> 16) {
                    case SHOP_CATEGORY_WEAPONS:
                        return Shop_Data_80181658;
                    case SHOP_CATEGORY_AMMUNITION:
                        return Shop_Data_80181668;
                    case SHOP_CATEGORY_ARMOR:
                        return Shop_Data_80181678;
                    case SHOP_CATEGORY_ITEMS:
                        return Shop_Data_80181680;
                }
                break;
            default:
                switch ((u32)stockSelector >> 16) {
                    case SHOP_CATEGORY_WEAPONS:
                        return Shop_Data_801815F8;
                    case SHOP_CATEGORY_AMMUNITION:
                        return Shop_Data_80181600;
                    case SHOP_CATEGORY_ARMOR:
                        return Shop_Data_80181608;
                    case SHOP_CATEGORY_ITEMS:
                        return Shop_Data_80181610;
                }
                break;
        }
    } else {
        switch ((u16)stockSelector) {
            case SHOP_STOCK_SHELTER:
                switch ((u32)stockSelector >> 16) {
                    case SHOP_CATEGORY_WEAPONS:
                        return Shop_Data_801818A4;
                    case SHOP_CATEGORY_AMMUNITION:
                        return Shop_Data_801818B0;
                    case SHOP_CATEGORY_ARMOR:
                        return Shop_Data_801818B8;
                    case SHOP_CATEGORY_ITEMS:
                        return Shop_Data_801818C4;
                }
            case SHOP_STOCK_SHELTER_ALTERNATE:
                switch ((u32)stockSelector >> 16) {
                    case SHOP_CATEGORY_WEAPONS:
                        return Shop_Data_801818D0;
                    case SHOP_CATEGORY_AMMUNITION:
                        return Shop_Data_801818DC;
                    case SHOP_CATEGORY_ARMOR:
                        return Shop_Data_801818E0;
                    case SHOP_CATEGORY_ITEMS:
                        return Shop_Data_801818EC;
                }
            case SHOP_STOCK_SHELTER_EVENT:
                switch ((u32)stockSelector >> 16) {
                    case SHOP_CATEGORY_WEAPONS:
                        return Shop_Data_801818F8;
                    case SHOP_CATEGORY_AMMUNITION:
                        return Shop_Data_80181904;
                    case SHOP_CATEGORY_ARMOR:
                        return Shop_Data_8018190C;
                    case SHOP_CATEGORY_ITEMS:
                        return Shop_Data_80181918;
                }
            case SHOP_STOCK_SHELTER_ALTERNATE_EVENT:
                switch ((u32)stockSelector >> 16) {
                    case SHOP_CATEGORY_WEAPONS:
                        return Shop_Data_80181924;
                    case SHOP_CATEGORY_AMMUNITION:
                        return Shop_Data_80181930;
                    case SHOP_CATEGORY_ARMOR:
                        return Shop_Data_80181938;
                    case SHOP_CATEGORY_ITEMS:
                        return Shop_Data_80181944;
                }
            case SHOP_STOCK_DRYFIELD:
                switch ((u32)stockSelector >> 16) {
                    case SHOP_CATEGORY_WEAPONS:
                        return Shop_Data_80181830;
                    case SHOP_CATEGORY_AMMUNITION:
                        return Shop_Data_80181838;
                    case SHOP_CATEGORY_ARMOR:
                        return Shop_Data_80181840;
                    case SHOP_CATEGORY_ITEMS:
                        return Shop_Data_80181848;
                }
                break;
            case SHOP_STOCK_DRYFIELD_LATE:
                switch ((u32)stockSelector >> 16) {
                    case SHOP_CATEGORY_WEAPONS:
                        return Shop_Data_8018187C;
                    case SHOP_CATEGORY_AMMUNITION:
                        return Shop_Data_80181888;
                    case SHOP_CATEGORY_ARMOR:
                        return Shop_Data_80181890;
                    case SHOP_CATEGORY_ITEMS:
                        return Shop_Data_80181898;
                }
                break;
            case SHOP_STOCK_ARMORY:
                switch ((u32)stockSelector >> 16) {
                    case SHOP_CATEGORY_WEAPONS:
                        return Shop_Data_80181854;
                    case SHOP_CATEGORY_AMMUNITION:
                        return Shop_Data_8018185C;
                    case SHOP_CATEGORY_ARMOR:
                        return Shop_Data_80181868;
                    case SHOP_CATEGORY_ITEMS:
                        return Shop_Data_80181870;
                }
                break;
            default:
                switch ((u32)stockSelector >> 16) {
                    case SHOP_CATEGORY_WEAPONS:
                        return Shop_Data_80181810;
                    case SHOP_CATEGORY_AMMUNITION:
                        return Shop_Data_80181814;
                    case SHOP_CATEGORY_ARMOR:
                        return Shop_Data_80181818;
                    case SHOP_CATEGORY_ITEMS:
                        return Shop_Data_80181820;
                }
                break;
        }
    }
    return Shop_Data_80181AD4;
}

/// Draws one row of the shop list and handles its input, recording the row's
/// id as the cursor item while the row is selected. Row 0xFFFE is greyed out
/// and unselectable unless `equipmentHasCarriedWeaponSupply` answers non-zero, and opens its
/// own panel; row 0xFFFC is greyed out while the scan holds item 0x8F. Any
/// other row is an item with its price, greyed out when `inventoryIsItemLimitReached`
/// reports its ownership limit reached; confirm opens the buy panel and button
/// 0x10 the item's detail panel.
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
       instead of being rematerialised at the inventoryGetItemQuantity call. */
    scan = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    if (prompt->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        Shop_Data_801819EC = itemId;
    }

    if (itemId == 0xFFFE) {
        status = obj->panel.control.word;
        if (((status >> 16) == 1) || (status == 1)) {
            if (prompt->selectedItemIndex == prompt->currentItemIndex) {
                uiSetPromptText(Shop_Data_80181A20, 0, 0);
            }
        }
        if (equipmentHasCarriedWeaponSupply() == 0) {
            prompt->colorRgb        = uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_DIMMED);
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
            uiSpawnObject(&Shop_Data_80181BD8, 0, 1, 1, obj);
            obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        }
        return;
    }

    if (itemId == 0xFFFC) {
        status = obj->panel.control.word;
        if (((status >> 16) == 1) || (status == 1)) {
            if (prompt->selectedItemIndex == prompt->currentItemIndex) {
                uiSetPromptText(Gp_StrEmpty, 0, 0);
            }
        }
        if (inventoryGetItemQuantity(scan, 0x8F) != 0) {
            blocked          = 1;
            prompt->colorRgb = uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_DIMMED);
        }
        textDrawUiLine(obj, prompt->rowTextX.signedValue, prompt->rowTextY.signedValue, Shop_Data_80181A1C, prompt->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
        if (prompt->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE && blocked == 0 && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
            child = uiSpawnObject(&Shop_Data_80181B84, itemId, 1, 1, obj);
            if (child != NULL) {
                uiPositionRowDialog(&(child)->panel, prompt, &(obj)->panel);
                obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            }
        }
        return;
    }

    price = Gp_ItemDescs[itemId].price;
    if (inventoryIsItemLimitReached(itemId) != 0) {
        blocked          = 1;
        prompt->colorRgb = uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_DIMMED);
    }
    if (prompt->actionResult != USER_INTERFACE_LIST_ACTION_SKIP_ROW) {
        status = obj->panel.control.word;
        if (((status >> 16) == 1) || (status == 1)) {
            if (prompt->selectedItemIndex == prompt->currentItemIndex) {
                itemMenuSetItemDescriptionPrompt(itemId);
                itemMenuSetPreviewItem(itemId, CD_COMMAND_DISPLAY_LOAD_MENU);
            }
        }
    }
    if (prompt->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (blocked == 0 && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            child2 = uiSpawnObject(&Shop_Data_80181B84, itemId, 1, 1, obj);
            if (child2 != NULL) {
                sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
                uiPositionRowDialog(&(child2)->panel, prompt, &(obj)->panel);
                obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_TRIANGLE) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            uiSpawnObject(&D_8010EAB4[45], itemId, 1, 1, obj);
            obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        }
    }
    itemMenuDrawItemRow(obj, prompt->rowTextX.signedValue, prompt->rowTextY.signedValue, itemId, prompt->colorRgb, 0);
    if ((u32)(itemId - INVENTORY_CONSUMABLE_ITEM_FIRST) < INVENTORY_CONSUMABLE_ITEM_COUNT) {
        /* Dead: emits the scaled index before the table base so the
           `addu` is index-first, matching the original. */
        scaled = itemId * 4;
        itemMenuDrawQuantity(obj, prompt->rowTextX.signedValue, prompt->rowTextY.signedValue, _inventoryGetConsumableStackInfo(itemId)->packQty, prompt->colorRgb);
    }
    textItoaUnsigned(buf, price);
    textDrawUiLine(obj, -prompt->rowTextX.signedValue, prompt->rowTextY.signedValue, buf, prompt->colorRgb, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
}

/// Appends an offered row or upgrades the first lower PE level of its ability.
///
/// `list` must be the list in the owning task's `_ShopItemListWork`; `rowId`
/// is a catalogue id or `SHOP_ROW_RECHARGE_SERVICE`. Exact duplicates return
/// immediately. A higher PE level replaces the first lower level, but a lower
/// level arriving after a higher one can append a second row of that ability.
/// M.I.S.T. excludes the MP5A5 variants, Grenade Pistol and Tactical Vest only
/// while scanning an existing row, so an empty list bypasses that exclusion.
/// Newly appended catalogue items are identified. The stock builder proves
/// at most 33 rows; callers must leave space in the 64-row allocation.
static void _shopAppendItemRow(UiList* list, UiObject* object, s32 rowId)
{
    Task*              ownerTask     = object->owner;
    s32                stockSelector = ownerTask->spawnArg1.value;
    _ShopItemListWork* work          = ownerTask->work;
    s32                rowIndex;

    // Exact duplicates and upward PE replacement preserve insertion order.
    for (rowIndex = 0; rowIndex < list->itemCount; rowIndex++) {
        s32 existingRowId = work->rowIds[rowIndex];
        s32 abilityIndex;

        if (existingRowId == rowId) {
            return;
        }
        if (((stockSelector & 0xFFFF) == SHOP_STOCK_MIST) &&
            (((u32)(rowId - SHOP_ITEM_MP5A5_FIRST) < SHOP_ITEM_MP5A5_COUNT) || (rowId == SHOP_ITEM_GRENADE_PISTOL) || (rowId == SHOP_ITEM_TACTICAL_VEST))) {
            return;
        }
        if (((u32)(rowId - SHOP_PE_ITEM_FIRST) < SHOP_PE_ITEM_COUNT) && ((u16)(existingRowId - SHOP_PE_ITEM_FIRST) < SHOP_PE_ITEM_COUNT)) {
            abilityIndex = (rowId - SHOP_PE_ITEM_FIRST) / SHOP_PE_LEVEL_COUNT;
            if ((abilityIndex == (existingRowId - SHOP_PE_ITEM_FIRST) / SHOP_PE_LEVEL_COUNT) && (((rowId - SHOP_PE_ITEM_FIRST) % SHOP_PE_LEVEL_COUNT + 1) > ((existingRowId - SHOP_PE_ITEM_FIRST) % SHOP_PE_LEVEL_COUNT + 1))) {
                work->rowIds[rowIndex] = rowId;
                return;
            }
        }
    }

    itemSetIdentified(rowId, 1);
    work->rowIds[list->itemCount] = rowId;
    list->itemCount++;
}

/// Sorts offered row ids by ascending inventory catalogue key.
static inline void _shopSortItemRows(_ShopItemListWork* work, const UiList* list)
{
    s32 rowIndex;
    s32 otherRowIndex;
    s32 sortKey;
    s32 otherSortKey;
    u16 swapRowId;

    for (rowIndex = 0; rowIndex < list->itemCount - 1; rowIndex++) {
        sortKey = inventoryGetItemSortKey(work->rowIds[rowIndex]);
        for (otherRowIndex = rowIndex + 1; otherRowIndex < list->itemCount; otherRowIndex++) {
            otherSortKey = inventoryGetItemSortKey(work->rowIds[otherRowIndex]);
            if (otherSortKey < sortKey) {
                swapRowId                   = work->rowIds[rowIndex];
                sortKey                     = otherSortKey;
                work->rowIds[rowIndex]      = work->rowIds[otherRowIndex];
                work->rowIds[otherRowIndex] = swapRowId;
            }
        }
    }
}

/// Builds and sorts a category's stock, including saved replay and PE unlocks.
///
/// `list` is the owning task's embedded list control. Normal/replay mode 0
/// adds unlocked tier items by category and the twelve packed two-bit PE
/// levels to Items; Pyrokinesis requires level 2. Demo scene 1 first writes
/// all tier and PE unlock bits to the live save. The complete stock/unlock
/// domain produces at most 33 rows. Visible rows cap at nine, and the preview
/// selection resets to `SHOP_PREVIEW_NONE`.
static void _shopBuildItemList(UiList* list, UiObject* object)
{
    _ShopItemListWork* work;
    const u16*         stockRows;
    s32                stockSelector;
    s32                tierIndex;
    s32                abilityIndex;
    s32                abilityLevel;
    s32                levelItemOffset;
    s32                itemId;
    s32                tierUnlocked;
    s32                tierItemIndex;
    u8                 rowCount;

    stockSelector = object->owner->spawnArg1.value;
    stockRows     = _shopSelectStockList(stockSelector);

    list->itemCount = 0;
    while (*stockRows != SHOP_ROW_END) {
        _shopAppendItemRow(list, object, *stockRows);
        stockRows++;
    }

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene == 1) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.shopTiers = SHOP_TIER_ALL_MASK;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.shopStock = SHOP_PE_STOCK_ALL_LEVELS;
    }

    // Replay bonuses are offered only in normal/replay mode.
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode == SHOP_GAME_MODE_NORMAL) {
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.shopTiers != 0) {
            for (tierIndex = 0; tierIndex < SHOP_TIER_COUNT; tierIndex++) {
                tierUnlocked = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.shopTiers & (1 << tierIndex);
                if (tierUnlocked != 0) {
                    for (tierItemIndex = 0; tierItemIndex < ARRAY_SIZE(Shop_Data_80181950[tierIndex].items); tierItemIndex++) {
                        itemId = Shop_Data_80181950[tierIndex].items[tierItemIndex];
                        switch (stockSelector >> 16) {
                            case SHOP_CATEGORY_WEAPONS:
                                if (((u32)(itemId - EQUIPMENT_WEAPON_ITEM_FIRST) < SHOP_WEAPON_ITEM_COUNT) || (itemId == SHOP_ITEM_SNAIL_MAGAZINE) || (itemId == SHOP_ITEM_SMG_CLIP_HOLDER) ||
                                    (itemId == SHOP_ITEM_RIFLE_CLIP_HOLDER) || (itemId == SHOP_ITEM_M9) || (itemId == SHOP_ITEM_M203) ||
                                    (itemId == SHOP_ITEM_HAMMER) || (itemId == SHOP_ITEM_PYKE) || (itemId == SHOP_ITEM_JAVELIN)) {
                                    _shopAppendItemRow(list, object, itemId);
                                }
                                break;
                            case SHOP_CATEGORY_AMMUNITION:
                                if ((u32)(itemId - INVENTORY_CONSUMABLE_ITEM_FIRST) < INVENTORY_CONSUMABLE_ITEM_COUNT) {
                                    _shopAppendItemRow(list, object, itemId);
                                }
                                break;
                            case SHOP_CATEGORY_ARMOR:
                                if (((u32)(itemId - SHOP_ARMOR_ITEM_FIRST) < SHOP_ARMOR_ITEM_COUNT) || (itemId == SHOP_ITEM_BELT_POUCH)) {
                                    _shopAppendItemRow(list, object, itemId);
                                }
                                break;
                            case SHOP_CATEGORY_ITEMS:
                                if (((u32)(itemId - 1) < 0x5FU) && (itemId != SHOP_ITEM_BELT_POUCH) && (itemId != SHOP_ITEM_SNAIL_MAGAZINE) &&
                                    (itemId != SHOP_ITEM_SMG_CLIP_HOLDER) && (itemId != SHOP_ITEM_RIFLE_CLIP_HOLDER) && (itemId != SHOP_ITEM_M9) &&
                                    (itemId != SHOP_ITEM_M203) && (itemId != SHOP_ITEM_HAMMER) && (itemId != SHOP_ITEM_PYKE) &&
                                    (itemId != SHOP_ITEM_JAVELIN)) {
                                    _shopAppendItemRow(list, object, itemId);
                                }
                                break;
                        }
                    }
                }
            }
        }

        if ((stockSelector >> 16) == SHOP_CATEGORY_ITEMS) {
            for (abilityIndex = 0; abilityIndex < SHOP_PE_ABILITY_COUNT; abilityIndex++) {
                abilityLevel = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.shopStock >> (abilityIndex * SHOP_PE_STOCK_BITS_PER_ABILITY)) & SHOP_PE_STOCK_LEVEL_MASK;
                if (abilityIndex == 0 ? abilityLevel >= 2 : abilityLevel > 0) {
                    levelItemOffset = abilityLevel + (SHOP_PE_ITEM_FIRST - 1);
                    _shopAppendItemRow(list, object, abilityIndex * SHOP_PE_LEVEL_COUNT + levelItemOffset);
                }
            }
        }
    }

    // Sort row ids together, then fit the scrolling viewport.
    work = object->owner->work;
    _shopSortItemRows(work, list);

    rowCount                            = list->itemCount;
    list->visibleRowCount.unsignedValue = rowCount;
    if ((s8)rowCount >= SHOP_ITEM_LIST_VISIBLE_ROWS + 1) {
        list->visibleRowCount.unsignedValue = SHOP_ITEM_LIST_VISIBLE_ROWS;
    }
    Shop_Data_801819EC = SHOP_PREVIEW_NONE;
}

static const u8 Shop_Data_8017D6D0[] = "Select";

static const u8 Shop_Data_8017D6D8[] = "BP";

static const u8 Shop_Data_8017D6DC[] = "List";

static const u8 Shop_Data_8017D6E4[] = "TOTAL";

static const u8 Shop_Data_8017D6EC[] = "Notice";

static const char Shop_Data_8017D6F4[8] = SHOP_CHARGE_TITLE_BYTES;

/// Runs the offered-item list and its preview and purchase child panels.
///
/// `spawnArg1` is the packed stock selector and `spawnArg2.pointer` borrows
/// the task-owned UI object. Initialization allocates `_ShopItemListWork`,
/// released by task teardown. Cancel returns CONFIRM to close this panel;
/// Menu propagates CANCEL through the shop. Child CONFIRM closes that child
/// and restores list input; child CANCEL propagates upward.
static void _shopItemListTask(Task* task)
{
    TextDrawReq        req;
    UiObject*          object;
    _ShopItemListWork* work;
    Task*              firstChild;
    Task*              childTask;
    Task*              nextChild;
    UiObject*          childObject;
    void*              allocation;
    s32                childResult;
    s32                captionOriginX;
    s32                captionOriginY;

    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&object->panel, (const char*)Shop_Data_8017D6D0);
    if (task->state == SHOP_PANEL_INITIAL) {
        allocation = memCalloc(sizeof(_ShopItemListWork), 0);
        if (allocation != NULL) {
            work                      = allocation;
            task->work                = work;
            work->list.rowCallbacks   = Shop_Data_80181AD8;
            work->list.wrapNavigation = 0;
            work->list.rowHeight      = 0xF;
            _shopBuildItemList(&work->list, object);
            uiFitPanelToList(&work->list, &object->panel);
            work->list.flags = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
            uiSetListSystemCursorSound(&work->list, 1);
            object->panel.bounds.unsignedRect.h += 8;
            work->list.topInset                  = 8;
            uiSpawnObject(&Shop_Data_80181BF4, 0, USER_INTERFACE_PANEL_INACTIVE, 0, object);
            task->state += 1;
        }
    }
    work = task->work;
    uiUpdateList(&work->list, &object->panel);
    uiDrawHorizontalSeparator(&object->panel, object->panel.contentLeft.signedValue, object->panel.contentRight.signedValue, object->panel.contentTop.signedValue + 6);

    captionOriginX = object->panel.contentOriginX.unsignedValue - 2;
    req.x          = object->panel.contentRight.unsignedValue + captionOriginX;
    captionOriginY = object->panel.contentOriginY.unsignedValue + 2;
    req.y          = object->panel.contentTop.unsignedValue + captionOriginY;
    req.otIndex    = object->panel.otIndex.signedValue + 1;
    req.colorRgb   = SHOP_TEXT_COLOR_RGB;
    req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req.alignment  = TEXT_ALIGNMENT_RIGHT;
    req.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req, Shop_Data_8017D6D8);

    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            object->result = USER_INTERFACE_RESULT_CANCEL;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
            object->result = USER_INTERFACE_RESULT_CONFIRM;
        }
    }

    firstChild = task->firstChild;
    if (firstChild != NULL) {
        childTask = firstChild;
        do {
            childObject = childTask->spawnArg2.pointer;
            childResult = childObject->result;
            nextChild   = childTask->nextSibling;
            if (childResult != USER_INTERFACE_RESULT_CANCEL) {
                if (childResult == USER_INTERFACE_RESULT_CONFIRM) {
                    uiStartTreeClosing(childObject, childObject->owner);
                    object->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
                }
            } else {
                object->result = childResult;
            }
            childTask = nextChild;
        } while (childTask != task->firstChild);
    }
}

/// Draws a category or Pass row and opens the selected category's item list.
///
/// The shared row callback uses `currentItemIndex` (0 Weapons, 1 Ammunition,
/// 2 Armor, 3 Items, last row Pass). It preserves the owning task's low-half
/// stock profile while replacing its high-half category. An empty stock set
/// dims and disables the category. Confirm on Pass returns CONFIRM to close
/// the shop; opening an item list suspends this panel's input.
static void _shopDrawCategoryRow(UiList* list, UiObject* object)
{
    const u8* label;
    s32       panelControl;

    if ((list->itemCount - 1) == list->currentItemIndex) {
        textDrawUiLine(object, list->rowTextX.signedValue, list->rowTextY.signedValue, Shop_Data_80181A04, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
        if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            object->result = USER_INTERFACE_RESULT_CONFIRM;
        }
        return;
    }

    label                          = Shop_Data_80181A5C;
    object->owner->spawnArg1.value = (u16)object->owner->spawnArg1.value;
    switch (list->currentItemIndex) {
        case SHOP_CATEGORY_WEAPONS:
            break;
        case SHOP_CATEGORY_AMMUNITION:
            label                           = Shop_Data_80181A64;
            object->owner->spawnArg1.value |= (SHOP_CATEGORY_AMMUNITION << 16);
            break;
        case SHOP_CATEGORY_ARMOR:
            label                           = Shop_Data_80181A70;
            object->owner->spawnArg1.value |= (SHOP_CATEGORY_ARMOR << 16);
            break;
        case SHOP_CATEGORY_ITEMS:
            label                           = Shop_Data_80181A78;
            object->owner->spawnArg1.value |= (SHOP_CATEGORY_ITEMS << 16);
            break;
    }

    if (*_shopSelectStockList(object->owner->spawnArg1.value) == SHOP_ROW_END) {
        list->colorRgb        = uiGetTextColor(object, USER_INTERFACE_TEXT_COLOR_DIMMED);
        list->rowInputEnabled = USER_INTERFACE_LIST_ROW_INACTIVE;
    }

    textDrawUiLine(object, list->rowTextX.signedValue, list->rowTextY.signedValue, label, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);

    panelControl = object->panel.control.word;
    if (((panelControl >> 16) == USER_INTERFACE_PANEL_ACTIVE) || (panelControl == USER_INTERFACE_PANEL_ACTIVE)) {
        if (list->selectedItemIndex == list->currentItemIndex) {
            uiSetPromptText((const u8*)Gp_StrEmpty, 0, 0);
        }
    }

    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
        sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
        uiSpawnObject(&Shop_Data_80181B4C, object->owner->spawnArg1, USER_INTERFACE_PANEL_ACTIVE, 1, object);
        object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
    }
}

/// Runs the shop's four-category menu and Pass row with balance and help panels.
///
/// `spawnArg1` carries the packed stock selector; `spawnArg2.pointer` borrows
/// its UI object. Initialization clears preview state and fits the shared
/// five-row control. Cancel/Menu reports CANCEL. Child CONFIRM closes the
/// child and restores input; child CANCEL clears the UI holder and propagates.
static void _shopCategoryListTask(Task* task)
{
    UiObject* object;
    UiList*   list;
    Task*     childTask;
    Task*     nextChild;
    Task*     firstChild;
    UiObject* childObject;
    s32       childResult;

    object         = task->spawnArg2.pointer;
    list           = &Shop_Data_80181AE0;
    object->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&object->panel, (const char*)Shop_Data_8017D6DC);
    if (task->state == SHOP_PANEL_INITIAL) {
        itemMenuClearPreviewItems();
        D_80067634 = NULL;
        uiSpawnObject(&Shop_Data_80181B68, task->spawnArg1, USER_INTERFACE_PANEL_INACTIVE, 1, object);
        uiSpawnObject(&D_8010D6F4[10], 0, USER_INTERFACE_PANEL_INACTIVE, 0, object);
        list->itemCount                     = SHOP_CATEGORY_COUNT + 1;
        list->visibleRowCount.unsignedValue = SHOP_CATEGORY_COUNT + 1;
        uiFitPanelToList(list, &object->panel);
        list->flags = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        uiSetListSystemCursorSound(list, 1);
        task->state += 1;
    }
    uiUpdateList(list, &object->panel);
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel | Pad_MaskMenu) != 0) {
        object->result = USER_INTERFACE_RESULT_CANCEL;
    }

    firstChild = task->firstChild;
    if (firstChild != NULL) {
        childTask = firstChild;
        do {
            childObject = childTask->spawnArg2.pointer;
            childResult = childObject->result;
            nextChild   = childTask->nextSibling;
            if (childResult != USER_INTERFACE_RESULT_CANCEL) {
                if (childResult == USER_INTERFACE_RESULT_CONFIRM) {
                    uiStartTreeClosing(childObject, childObject->owner);
                    object->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
                }
            } else {
                Wip_UiHolder   = NULL;
                object->result = childResult;
            }
            childTask = nextChild;
        } while (childTask != task->firstChild);
    }
}

/// Draws the player's BP balance and occupied carried rows over row capacity.
///
/// `spawnArg2.pointer` borrows the panel object. Counts refer to inventory
/// slots, including stacks as one occupied slot, rather than item units.
static void _shopBalancePanelTask(Task* task)
{
    u8                        balanceText[0x20];
    u8                        capacityText[0x20];
    TextDrawReq               balanceLabelRequest;
    TextDrawReq               capacityLabelRequest;
    UiObject*                 object;
    const PlayerStatus*       playerStatus;
    const InventoryItemRange* carriedItems;
    u8*                       textCursor;
    s32                       labelLeft;
    s32                       contentTop;
    s32                       capacityTop;
    s32                       valueRight;
    s32                       capacity;
    s32                       occupiedRows;

    object       = task->spawnArg2.pointer;
    playerStatus = &gPlayerStatus;
    labelLeft    = object->panel.contentLeft.signedValue + 2;
    valueRight   = object->panel.contentRight.signedValue - 2;
    contentTop   = object->panel.contentTop.signedValue;

    balanceLabelRequest.x          = object->panel.contentOriginX.unsignedValue + labelLeft;
    balanceLabelRequest.y          = object->panel.contentOriginY.unsignedValue + contentTop + 9;
    balanceLabelRequest.otIndex    = object->panel.otIndex.signedValue + 1;
    balanceLabelRequest.colorRgb   = SHOP_TEXT_COLOR_RGB;
    balanceLabelRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    balanceLabelRequest.alignment  = TEXT_ALIGNMENT_LEFT;
    balanceLabelRequest.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&balanceLabelRequest, Shop_Data_8017D6D8);

    textItoaUnsigned(balanceText, playerStatus->bp);
    textDrawUiLine(object, valueRight, contentTop + 0x19, balanceText, SHOP_TEXT_COLOR_RGB, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);

    capacityTop                     = contentTop + 0x28;
    capacityLabelRequest.x          = object->panel.contentOriginX.unsignedValue + labelLeft;
    capacityLabelRequest.y          = object->panel.contentOriginY.unsignedValue + (capacityTop - 6);
    capacityLabelRequest.otIndex    = object->panel.otIndex.signedValue + 1;
    capacityLabelRequest.colorRgb   = SHOP_TEXT_COLOR_RGB;
    capacityLabelRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    capacityLabelRequest.alignment  = TEXT_ALIGNMENT_LEFT;
    capacityLabelRequest.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&capacityLabelRequest, Shop_Data_8017D6E4);

    textCursor   = capacityText;
    carriedItems = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    occupiedRows = inventoryCountOccupiedRows(carriedItems);
    capacity     = carriedItems->rowCount;
    textItoaUnsigned(textCursor, occupiedRows);
    while (*(const s8*)textCursor != 0) {
        textCursor++;
    }
    *textCursor = '/';
    textItoaUnsigned(textCursor + 1, capacity);
    textDrawUiLine(object, valueRight, capacityTop + 0xA, capacityText, SHOP_TEXT_COLOR_RGB, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
}

/// Draws Purchase and handles the affordability and inventory-capacity checks.
///
/// The owning task's `spawnArg1` is the item id. Confirm shows insufficient
/// BP, full-inventory or full-ammunition notices as appropriate. Ammunition
/// category 1 opens a pack-quantity picker; other categories deduct one pack's
/// price, grant one pack and return CONFIRM. A child dialog suspends input.
static void _shopDrawPurchaseRow(UiList* list, UiObject* object)
{
    TextDrawReq         request;
    UiObject*           quantityObject;
    PlayerStatus*       playerStatus;
    InventoryItemRange* carriedItems;
    s32                 itemId;
    s32                 packPrice;

    itemId = object->owner->spawnArg1.value;

    request.x          = object->panel.contentOriginX.unsignedValue + list->rowTextX.unsignedValue;
    request.y          = object->panel.contentOriginY.unsignedValue + list->rowTextY.unsignedValue;
    request.otIndex    = object->panel.otIndex.signedValue + 1;
    request.colorRgb   = list->colorRgb;
    request.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    request.alignment  = TEXT_ALIGNMENT_LEFT;
    request.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&request, Shop_Data_801819F0);

    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
        playerStatus = &gPlayerStatus;
        packPrice    = Gp_ItemDescs[itemId].price;
        carriedItems = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
        sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
        if (playerStatus->bp >= packPrice) {
            if (inventoryCanAddItem(carriedItems, itemId) == 0) {
                if ((u32)(itemId - INVENTORY_CONSUMABLE_ITEM_FIRST) < INVENTORY_CONSUMABLE_ITEM_COUNT && inventoryGetItemQuantity(carriedItems, itemId) != 0) {
                    uiSpawnObject(&Shop_Data_80181BA0, SHOP_NOTICE_AMMUNITION_CAPACITY, USER_INTERFACE_PANEL_ACTIVE, 1, object);
                } else {
                    uiSpawnObject(&Shop_Data_80181BA0, SHOP_NOTICE_INVENTORY_FULL, USER_INTERFACE_PANEL_ACTIVE, 1, object);
                }
                object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            } else if ((object->owner->parent->spawnArg1.value >> 16) == SHOP_CATEGORY_AMMUNITION) {
                quantityObject = uiSpawnObject(&Shop_Data_80181C10, itemId, USER_INTERFACE_PANEL_ACTIVE, 1, object);
                if (quantityObject != NULL) {
                    uiPositionRowDialog(&quantityObject->panel, list, &object->panel);
                    object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                }
            } else {
                playerStatus->bp -= packPrice;
                inventoryGiveItem(carriedItems, itemId, INVENTORY_GIVE_ONE_PACK);
                object->result = USER_INTERFACE_RESULT_CONFIRM;
            }
        } else {
            uiSpawnObject(&Shop_Data_80181BA0, SHOP_NOTICE_INSUFFICIENT_BP, USER_INTERFACE_PANEL_ACTIVE, 1, object);
            object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        }
    }
}

/// Shows a timed purchase-failure notice, then dismisses the purchase prompt.
///
/// `spawnArg1` selects full inventory (1), ammunition capacity (2), or
/// insufficient BP (all other values). `spawnArg2.pointer` borrows the notice
/// object. The countdown uses nominal 60-Hz display ticks. Once input is
/// active, Confirm/Cancel or expiry sets the parent object's result to
/// CONFIRM; Menu returns CANCEL through the child-result path.
static void _shopNoticeTask(Task* task)
{
    UiObject* object;
    UiObject* parentObject;
    const u8* message;
    s32       noticeId;

    noticeId = task->spawnArg1.value;
    object   = task->spawnArg2.pointer;
    switch (noticeId) {
        case SHOP_NOTICE_INVENTORY_FULL:
            message = Shop_Data_80181A94;
            break;
        case SHOP_NOTICE_AMMUNITION_CAPACITY:
            message = Shop_Data_80181AA4;
            break;
        default:
            message = Shop_Data_80181A80;
            break;
    }

    uiDrawPanelLabel(&object->panel, (const char*)Shop_Data_8017D6EC);
    object->result = USER_INTERFACE_RESULT_NONE;
    if (task->state == SHOP_PANEL_INITIAL) {
        uiSizePanelForTextDefault(&object->panel, message);
        task->killCountdown = SHOP_NOTICE_DURATION_TICKS;
        task->state        += 1;
    }
    textDrawUiLines(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 0xF, message, SHOP_TEXT_COLOR_RGB, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    task->killCountdown -= gDisplayState.frameTicks;
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            object->result = USER_INTERFACE_RESULT_CANCEL;
            return;
        }
        if (task->killCountdown <= 0 || padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
            parentObject         = task->parent->spawnArg2.pointer;
            parentObject->result = USER_INTERFACE_RESULT_CONFIRM;
            task->killCountdown  = SHOP_NOTICE_DISMISSED_COUNTDOWN;
        }
    }
}

/// Fully refills each carried weapon's built-in supply and animates its charge.
///
/// `spawnArg2.pointer` borrows the UI object. The service requires at least
/// one carried rechargeable weapon and keeps the current supply and animation
/// quantity in this room's shared refill storage. The actual load reaches
/// capacity immediately; its display advances by a quarter-unit per callback
/// in eight-fractional-bit units. Active Confirm/Cancel or 188 callbacks moves
/// to the next supply. Exhaustion returns CONFIRM; it still draws the last
/// supply while the panel closes. No BP or carried item stack is consumed.
static void _shopRefillWeaponSupplyTask(Task* task)
{
    UiObject*                    object;
    const EquipmentWeaponSupply* supply;
    EquipmentWeaponLoad*         weaponLoad;
    s32                          supplyIndex;
    s32                          refillWeaponItemId;
    s32                          weaponItemId;
    s32                          supplyItemId;
    s32                          capacityFixed;
    s32                          contentTop;
    s32                          contentBottom;
    s32                          panelControl;
    s16                          remainingFrames;

    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&object->panel, Shop_Data_8017D6F4);

    if (task->state == SHOP_REFILL_INITIAL) {
        task->spawnArg1.value = 0;
        task->state           = task->state + 1;
    }
    // Commit the full load before its displayed quantity begins rising.
    if (task->state == SHOP_REFILL_NEXT_SUPPLY) {
        supplyIndex           = equipmentFindNextCarriedWeaponSupply(task->spawnArg1.value);
        task->spawnArg1.value = supplyIndex;
        if (supplyIndex < 0) {
            object->result = USER_INTERFACE_RESULT_CONFIRM;
        } else {
            supply             = equipmentGetWeaponSupply(supplyIndex);
            Shop_Data_8018762C = supply;
            refillWeaponItemId = supply->weaponItemId;
            weaponLoad         = equipmentGetWeaponLoad(refillWeaponItemId);
            if (Shop_Data_8018762C->supplyLoad == EQUIPMENT_WEAPON_SUPPLY_PRIMARY) {
                Shop_Data_80187628     = weaponLoad->primaryQty;
                weaponLoad->primaryQty = equipmentGetWeaponLoadCapacity(refillWeaponItemId, EQUIPMENT_WEAPON_SUPPLY_PRIMARY);
            } else {
                Shop_Data_80187628       = weaponLoad->secondaryQty;
                weaponLoad->secondaryQty = equipmentGetWeaponLoadCapacity(refillWeaponItemId, EQUIPMENT_WEAPON_SUPPLY_SECONDARY);
            }
            task->killCountdown  = SHOP_REFILL_DURATION_FRAMES;
            Shop_Data_80187628 <<= SHOP_REFILL_FRACTION_BITS;
            task->state          = task->state + 1;
        }
    }

    // Keep drawing the last selected supply during completion and closure.
    weaponItemId = Shop_Data_8018762C->weaponItemId;
    supplyItemId = Shop_Data_8018762C->supplyItemId;
    if (Shop_Data_8018762C->supplyLoad == EQUIPMENT_WEAPON_SUPPLY_PRIMARY) {
        capacityFixed = equipmentGetWeaponLoadCapacity(weaponItemId, EQUIPMENT_WEAPON_SUPPLY_PRIMARY);
    } else {
        capacityFixed = equipmentGetWeaponLoadCapacity(weaponItemId, EQUIPMENT_WEAPON_SUPPLY_SECONDARY);
    }
    capacityFixed     <<= SHOP_REFILL_FRACTION_BITS;
    Shop_Data_80187628 += SHOP_REFILL_STEP_FIXED;
    if (capacityFixed < Shop_Data_80187628) {
        Shop_Data_80187628 = capacityFixed;
    }

    contentTop = object->panel.contentTop.signedValue;
    itemMenuDrawItemRow(object, object->panel.contentLeft.signedValue + 2, contentTop + 0xF, weaponItemId, SHOP_TEXT_COLOR_RGB, 0);
    uiDrawHorizontalSeparator(&object->panel, object->panel.contentLeft.signedValue, object->panel.contentRight.signedValue, contentTop + 0x12);
    itemMenuDrawItemRow(object, object->panel.contentLeft.signedValue + 2, contentTop + 0x23, supplyItemId, SHOP_TEXT_COLOR_RGB, 0);
    itemMenuDrawQuantity(object, object->panel.contentLeft.signedValue + 2, contentTop + 0x23, Shop_Data_80187628 >> SHOP_REFILL_FRACTION_BITS, SHOP_TEXT_COLOR_RGB);
    contentBottom = object->panel.contentBottom.signedValue;
    itemMenuDrawMeter(&object->panel, object->panel.contentLeft.signedValue + 2, object->panel.contentRight.signedValue - 2, contentBottom - 6, capacityFixed,
                      Shop_Data_80187628, SHOP_REFILL_METER_COLOR_RGB);

    if (task->state == SHOP_REFILL_ANIMATE) {
        remainingFrames     = task->killCountdown - 1;
        task->killCountdown = remainingFrames;
        panelControl        = object->panel.control.word;
        if (panelControl == USER_INTERFACE_PANEL_ACTIVE && (remainingFrames <= 0 || padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel | Pad_MaskConfirm) != 0)) {
            task->state           = panelControl;
            task->spawnArg1.value = task->spawnArg1.value + 1;
        }
    }
}

/// Adds carried consumable units or matching non-consumable rows to a count.
///
/// Consumable ids 0xA0..0xBF use the first stack's signed quantity, including
/// loaded ammunition. Other ids count matching rows without summing their
/// quantities. The live save's carried range must fit its readable table;
/// the returned count includes `accumulatedCount`. Nothing is changed or retained.
static inline s32 _shopAddCarriedItemCount(s32 itemId, s32 accumulatedCount)
{
    s32                       rowIndex;
    s32                       rowCount;
    const InventoryItemRow*   rows;
    const InventoryItemRange* carriedItems;

    if ((u32)(itemId - INVENTORY_CONSUMABLE_ITEM_FIRST) < INVENTORY_CONSUMABLE_ITEM_COUNT) {
        accumulatedCount += inventoryGetConsumableStackQuantity(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, itemId);
    } else {
        carriedItems = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
        rows         = inventoryGetRangeTable(carriedItems) + carriedItems->firstRow;
        rowCount     = carriedItems->rowCount;
        for (rowIndex = 0; rowIndex < rowCount; rowIndex++) {
            if (rows[rowIndex].itemId == itemId) {
                accumulatedCount++;
            }
        }
    }
    return accumulatedCount;
}

/// Draws the selected item's loaded preview and the carried Amount caption.
///
/// `spawnArg2.pointer` borrows the panel object. The preview stays hidden until
/// disk commands are idle and the loaded slot agrees with the selected row.
/// Row ids below 0x100 display the carried count; the empty selection -1 also
/// follows that path and displays zero. Service rows omit the amount caption.
static void _shopItemPreviewTask(Task* task)
{
    u8          quantityText[0x10];
    TextDrawReq amountRequest;
    UiObject*   object;
    s32         rowId;
    s32         amountTop;
    s32         captionOffsetY;
    s32         heldCount;

    rowId        = Shop_Data_801819EC;
    object       = task->spawnArg2.pointer;
    task->status = 0;
    if (cdCmdIsIdle() && Shop_Data_801819EC == itemMenuGetPrimaryPreviewItem()) {
        itemMenuDrawPreview(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 2, ITEM_MENU_PREVIEW_SCALE_SHOP);
    } else {
        itemMenuDrawPreview(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 2, ITEM_MENU_PREVIEW_SCALE_SHOP | ITEM_MENU_PREVIEW_HIDDEN);
    }
    amountTop = object->panel.contentTop.signedValue + 0x50;
    if (rowId < SHOP_CARRIED_ITEM_ID_LIMIT) {
        amountRequest.x          = object->panel.contentLeft.signedValue + (object->panel.contentOriginX.unsignedValue + 2);
        captionOffsetY           = object->panel.contentOriginY.unsignedValue - 6;
        amountRequest.y          = captionOffsetY + amountTop;
        amountRequest.otIndex    = object->panel.otIndex.signedValue + 1;
        amountRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
        amountRequest.colorRgb   = SHOP_TEXT_COLOR_RGB;
        amountRequest.alignment  = TEXT_ALIGNMENT_LEFT;
        amountRequest.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&amountRequest, Shop_Data_80181AC4);
        heldCount = 0;
        heldCount = _shopAddCarriedItemCount(rowId, heldCount);
        textDrawUiLine(object, object->panel.contentRight.signedValue - 2, amountTop + 0xA, textItoaSigned(quantityText, heldCount), SHOP_TEXT_COLOR_RGB, TEXT_DRAW_TRANSLUCENT_OUTLINED,
                       TEXT_ALIGNMENT_RIGHT);
    }
}

/// Picks a number of packs, shows their BP cost, and grants them on confirmation.
///
/// `spawnArg1` is an affordable item id from a validated purchase row;
/// `spawnArg2.pointer` borrows this task's UI object. `extraState.value` holds
/// the selected pack count, starting at one. The catalogue price must be
/// positive. Consumable capacity limits packs
/// with upward rounding, allowing the last pack's inventory clamp; other items
/// use free carried rows. BP affordability further caps the count. Active
/// Up/Right and Down/Left adjust it; Confirm deducts BP and grants packs.
/// Confirm and Cancel both set the parent result to CONFIRM to close it.
static void _shopPurchaseQuantityTask(Task* task)
{
    u8          numberText[0x20];
    TextDrawReq priceLabelRequest;
    UiObject*   object;
    UiObject*   parentObject;
    s32         itemId;
    s32         packPrice;
    s32         maxPacks;
    s32         affordablePacks;
    s32         heldUnits;
    s32         purchasePacks;
    s32         contentLeft;
    s32         contentTop;
    s32         rowLeft;
    s32         lineY;
    s32         packIndex;

    itemId    = task->spawnArg1.value;
    object    = task->spawnArg2.pointer;
    maxPacks  = 1;
    packPrice = Gp_ItemDescs[itemId].price;

    if (task->state == SHOP_PANEL_INITIAL) {
        task->extraState.value = 1;
        uiSetPanelContentSize(&object->panel, 0, uiGetTextRowsHeight(3) - 3);
        task->state = task->state + 1;
    }

    if ((u32)(itemId - INVENTORY_CONSUMABLE_ITEM_FIRST) < INVENTORY_CONSUMABLE_ITEM_COUNT) {
        const InventoryConsumableStack* stackLimits = _inventoryGetConsumableStackInfo(itemId);

        if (stackLimits->packQty != 0) {
            heldUnits = inventoryGetConsumableStackQuantity(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, itemId);
            // The duplicated arms preserve a vanished block boundary before the capacity load.
            // The original predicate is unproven; both arms have the same arithmetic.
            if (heldUnits > 0) {
                maxPacks = stackLimits->maxHeld - heldUnits;
            } else {
                maxPacks = stackLimits->maxHeld - heldUnits;
            }
            if (maxPacks <= 0) {
                maxPacks = 1;
            } else {
                maxPacks = (maxPacks - 1) / stackLimits->packQty;
                maxPacks = maxPacks + 1;
            }
        }
    } else {
        maxPacks = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems.rowCount - inventoryCountOccupiedRows(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems);
    }

    affordablePacks = gPlayerStatus.bp / packPrice;
    if (affordablePacks < maxPacks) {
        maxPacks = affordablePacks;
    }

    contentLeft = object->panel.contentLeft.signedValue;
    rowLeft     = contentLeft + 2;
    contentTop  = object->panel.contentTop.signedValue;
    lineY       = contentTop + 0xF;
    itemMenuDrawItemRow(object, rowLeft, lineY, itemId, SHOP_TEXT_COLOR_RGB, 0);
    if ((u32)(itemId - INVENTORY_CONSUMABLE_ITEM_FIRST) < INVENTORY_CONSUMABLE_ITEM_COUNT) {
        const InventoryConsumableStack* stackLimits = _inventoryGetConsumableStackInfo(itemId);

        itemMenuDrawQuantity(object, rowLeft, lineY, stackLimits->packQty, SHOP_TEXT_COLOR_RGB);
    }

    purchasePacks = task->extraState.value;
    textDrawUiLine(object, contentLeft + 0x98, lineY, Shop_Data_80181AD0, SHOP_TEXT_COLOR_RGB, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
    textDrawUiLine(object, -rowLeft, lineY, textItoaSigned(numberText, purchasePacks), SHOP_TEXT_COLOR_RGB, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
    uiDrawHorizontalSeparator(&object->panel, contentLeft, -rowLeft + 2, contentTop + 0x12);

    priceLabelRequest.x          = object->panel.contentOriginX.unsignedValue - rowLeft;
    lineY                        = contentTop + 0x1A;
    priceLabelRequest.y          = object->panel.contentOriginY.unsignedValue + lineY;
    priceLabelRequest.otIndex    = object->panel.otIndex.signedValue + 1;
    priceLabelRequest.colorRgb   = SHOP_TEXT_COLOR_RGB;
    priceLabelRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    priceLabelRequest.alignment  = TEXT_ALIGNMENT_RIGHT;
    priceLabelRequest.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&priceLabelRequest, Shop_Data_8017D6D8);

    textDrawUiLine(object, -rowLeft, contentTop + 0x2B, textItoaSigned(numberText, purchasePacks * packPrice), SHOP_TEXT_COLOR_RGB, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);

    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        parentObject = task->parent->spawnArg2.pointer;
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_UP | PAD_BUTTON_RIGHT) != 0) {
            if (task->extraState.value < maxPacks) {
                task->extraState.value = task->extraState.value + 1;
                sndEvtRequestScriptStart(SOUND_SYSTEM_CURSOR, 0, 0);
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_DOWN | PAD_BUTTON_LEFT) != 0) {
            if (task->extraState.value >= 2) {
                task->extraState.value = task->extraState.value - 1;
                sndEvtRequestScriptStart(SOUND_SYSTEM_CURSOR, 0, 0);
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            gPlayerStatus.bp -= packPrice * task->extraState.value;
            for (packIndex = 0; packIndex < task->extraState.value; packIndex++) {
                inventoryGiveItem(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, itemId, INVENTORY_GIVE_ONE_PACK);
            }
            sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
            parentObject->result = USER_INTERFACE_RESULT_CONFIRM;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
            parentObject->result = USER_INTERFACE_RESULT_CONFIRM;
        }
    }
}

/// Draws Pass and returns CONFIRM when the active row receives Confirm.
static void _shopDrawPassRow(UiList* list, UiObject* object)
{
    TextDrawReq request;

    request.x          = object->panel.contentOriginX.unsignedValue + list->rowTextX.unsignedValue;
    request.y          = object->panel.contentOriginY.unsignedValue + list->rowTextY.unsignedValue;
    request.otIndex    = object->panel.otIndex.signedValue + 1;
    request.colorRgb   = list->colorRgb;
    request.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    request.alignment  = TEXT_ALIGNMENT_LEFT;
    request.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&request, Shop_Data_80181A04);

    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
        sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
        object->result = USER_INTERFACE_RESULT_CONFIRM;
    }
}

/// Runs the two-row Purchase/Pass prompt and closes completed child dialogs.
///
/// `spawnArg1` carries the offered item id; `spawnArg2.pointer` borrows the
/// UI object. Cancel returns CONFIRM and Menu returns CANCEL. A child returning
/// CONFIRM closes and restores this prompt's input; child CANCEL propagates.
static void _shopPurchasePromptTask(Task* task)
{
    UiObject* object;
    UiList*   list;
    Task*     childTask;
    UiObject* childObject;
    s16       childResult;

    list           = &Shop_Data_80181B0C;
    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    if (task->state == SHOP_PANEL_INITIAL) {
        uiFitPanelToList(list, &object->panel);
        uiSetListSystemCursorSound(list, 1);
        task->state += 1;
    }
    uiUpdateList(list, &object->panel);
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
            object->result = USER_INTERFACE_RESULT_CONFIRM;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu) != 0) {
            object->result = USER_INTERFACE_RESULT_CANCEL;
        }
    }

    childTask = task->firstChild;
    if (childTask != NULL) {
        childObject = childTask->spawnArg2.pointer;
        childResult = childObject->result;
        if (childResult != USER_INTERFACE_RESULT_CANCEL) {
            if (childResult == USER_INTERFACE_RESULT_CONFIRM) {
                uiStartTreeClosing(childObject, childObject->owner);
                object->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
            }
        } else {
            object->result = USER_INTERFACE_RESULT_CANCEL;
        }
    }
}

/// Owns a shop UI session and restores display timing after its tree closes.
///
/// `spawnArg1` is the packed stock selector. The session acquires the stage's
/// primitive buffer and stores its root UI object in `spawnArg2.pointer`.
/// While open it selects every-VBlank timing and sets the session UI-open flag.
/// Either root result starts tree closure and a ten-callback delay; completion
/// restores two-VBlank timing, clears the flag, kills the task, releases the
/// primitive buffer and requests exit from the stage mode task.
static void _shopSessionTask(Task* task)
{
    UiObject* object;

    if (task->state == SHOP_SESSION_INITIAL) {
        stageEnsureHeapTaskPrimitiveBuffer();
        object = uiSpawnObject(&Shop_Data_80181B30, task->spawnArg1, USER_INTERFACE_PANEL_ACTIVE, 1, NULL);
        if (object == NULL) {
            return;
        }
        displaySetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
        gGameSession->uiOpen    = 1;
        task->spawnArg2.pointer = object;
        task->state++;
    }

    if (task->state == SHOP_SESSION_OPEN) {
        object = task->spawnArg2.pointer;
        if (object->result == USER_INTERFACE_RESULT_CANCEL || object->result == USER_INTERFACE_RESULT_CONFIRM) {
            uiStartTreeClosing(object, object->owner);
            task->killCountdown = SHOP_SESSION_CLOSE_FRAMES;
            task->state         = SHOP_SESSION_CLOSING;
        }
    }

    // Let the closing tree finish before releasing its drawing storage.
    if (task->state == SHOP_SESSION_CLOSING) {
        task->killCountdown--;
        if (task->killCountdown <= 0) {
            displaySetFrameTiming(DISPLAY_TIMING_TWO_VBLANKS);
            gGameSession->uiOpen = 0;
            taskKill(task);
            stageReleaseTaskPrimitiveBuffer();
            stageRequestModeTaskExit();
        }
    }
}
