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

/// Catalog groups and individual accessories used to partition unlocked stock.
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
    SHOP_ITEM_SNAIL_MAGAZINE       = 0xC,
    SHOP_ARMOR_ITEM_FIRST          = 0x60,
    SHOP_ARMOR_ITEM_COUNT          = 0x20,
    SHOP_ITEM_TACTICAL_VEST        = 0x65,
    SHOP_WEAPON_ITEM_COUNT         = 0x20,
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
    SHOP_TEXT_COLOR_RGB             = 0x606060
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

static void _shopDrawItemRow(UiList* list, UiObject* object);

/// Borrows a consumable's pack quantity and per-stack capacity from the catalog.
///
/// `consumableItemId` must be 0xA0..0xBF; no bounds check is performed.
/// Both quantities count item units. The read-only row remains valid while
/// the gameplay image is loaded; no inventory contents are read or changed.
static inline const InventoryConsumableStack* _inventoryGetConsumableStackInfo(s32 consumableItemId)
{
    return &gInventoryConsumableStacks[consumableItemId - INVENTORY_CONSUMABLE_ITEM_FIRST];
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
                        return _gShopStandardShelterWeaponsStock;
                    case SHOP_CATEGORY_AMMUNITION:
                        return _gShopStandardShelterAmmunitionStock;
                    case SHOP_CATEGORY_ARMOR:
                        return _gShopStandardShelterArmorStock;
                    case SHOP_CATEGORY_ITEMS:
                        return _gShopStandardShelterItemsStock;
                }
            case SHOP_STOCK_SHELTER_ALTERNATE:
                switch ((u32)stockSelector >> 16) {
                    case SHOP_CATEGORY_WEAPONS:
                        return _gShopStandardShelterAlternateWeaponsStock;
                    case SHOP_CATEGORY_AMMUNITION:
                        return _gShopStandardShelterAlternateAmmunitionStock;
                    case SHOP_CATEGORY_ARMOR:
                        return _gShopStandardShelterAlternateArmorStock;
                    case SHOP_CATEGORY_ITEMS:
                        return _gShopStandardShelterAlternateItemsStock;
                }
            case SHOP_STOCK_SHELTER_EVENT:
                switch ((u32)stockSelector >> 16) {
                    case SHOP_CATEGORY_WEAPONS:
                        return _gShopStandardShelterEventWeaponsStock;
                    case SHOP_CATEGORY_AMMUNITION:
                        return _gShopStandardShelterEventAmmunitionStock;
                    case SHOP_CATEGORY_ARMOR:
                        return _gShopStandardShelterEventArmorStock;
                    case SHOP_CATEGORY_ITEMS:
                        return _gShopStandardShelterEventItemsStock;
                }
            case SHOP_STOCK_SHELTER_ALTERNATE_EVENT:
                switch ((u32)stockSelector >> 16) {
                    case SHOP_CATEGORY_WEAPONS:
                        return _gShopStandardShelterAlternateEventWeaponsStock;
                    case SHOP_CATEGORY_AMMUNITION:
                        return _gShopStandardShelterAlternateEventAmmunitionStock;
                    case SHOP_CATEGORY_ARMOR:
                        return _gShopStandardShelterAlternateEventArmorStock;
                    case SHOP_CATEGORY_ITEMS:
                        return _gShopStandardShelterAlternateEventItemsStock;
                }
            case SHOP_STOCK_DRYFIELD:
                switch ((u32)stockSelector >> 16) {
                    case SHOP_CATEGORY_WEAPONS:
                        return _gShopStandardDryfieldWeaponsStock;
                    case SHOP_CATEGORY_AMMUNITION:
                        return _gShopStandardDryfieldAmmunitionStock;
                    case SHOP_CATEGORY_ARMOR:
                        return _gShopStandardDryfieldArmorStock;
                    case SHOP_CATEGORY_ITEMS:
                        return _gShopStandardDryfieldItemsStock;
                }
                break;
            case SHOP_STOCK_DRYFIELD_LATE:
                switch ((u32)stockSelector >> 16) {
                    case SHOP_CATEGORY_WEAPONS:
                        return _gShopStandardDryfieldLateWeaponsStock;
                    case SHOP_CATEGORY_AMMUNITION:
                        return _gShopStandardDryfieldLateAmmunitionStock;
                    case SHOP_CATEGORY_ARMOR:
                        return _gShopStandardDryfieldLateArmorStock;
                    case SHOP_CATEGORY_ITEMS:
                        return _gShopStandardDryfieldLateItemsStock;
                }
                break;
            case SHOP_STOCK_ARMORY:
                switch ((u32)stockSelector >> 16) {
                    case SHOP_CATEGORY_WEAPONS:
                        return _gShopStandardArmoryWeaponsStock;
                    case SHOP_CATEGORY_AMMUNITION:
                        return _gShopStandardArmoryAmmunitionStock;
                    case SHOP_CATEGORY_ARMOR:
                        return _gShopStandardArmoryArmorStock;
                    case SHOP_CATEGORY_ITEMS:
                        return _gShopStandardArmoryItemsStock;
                }
                break;
            default:
                switch ((u32)stockSelector >> 16) {
                    case SHOP_CATEGORY_WEAPONS:
                        return _gShopStandardDefaultWeaponsStock;
                    case SHOP_CATEGORY_AMMUNITION:
                        return _gShopStandardDefaultAmmunitionStock;
                    case SHOP_CATEGORY_ARMOR:
                        return _gShopStandardDefaultArmorStock;
                    case SHOP_CATEGORY_ITEMS:
                        return _gShopStandardDefaultItemsStock;
                }
                break;
        }
    } else {
        switch ((u16)stockSelector) {
            case SHOP_STOCK_SHELTER:
                switch ((u32)stockSelector >> 16) {
                    case SHOP_CATEGORY_WEAPONS:
                        return _gShopRestrictedShelterWeaponsStock;
                    case SHOP_CATEGORY_AMMUNITION:
                        return _gShopRestrictedShelterAmmunitionStock;
                    case SHOP_CATEGORY_ARMOR:
                        return _gShopRestrictedShelterArmorStock;
                    case SHOP_CATEGORY_ITEMS:
                        return _gShopRestrictedShelterItemsStock;
                }
            case SHOP_STOCK_SHELTER_ALTERNATE:
                switch ((u32)stockSelector >> 16) {
                    case SHOP_CATEGORY_WEAPONS:
                        return _gShopRestrictedShelterAlternateWeaponsStock;
                    case SHOP_CATEGORY_AMMUNITION:
                        return _gShopRestrictedShelterAlternateAmmunitionStock;
                    case SHOP_CATEGORY_ARMOR:
                        return _gShopRestrictedShelterAlternateArmorStock;
                    case SHOP_CATEGORY_ITEMS:
                        return _gShopRestrictedShelterAlternateItemsStock;
                }
            case SHOP_STOCK_SHELTER_EVENT:
                switch ((u32)stockSelector >> 16) {
                    case SHOP_CATEGORY_WEAPONS:
                        return _gShopRestrictedShelterEventWeaponsStock;
                    case SHOP_CATEGORY_AMMUNITION:
                        return _gShopRestrictedShelterEventAmmunitionStock;
                    case SHOP_CATEGORY_ARMOR:
                        return _gShopRestrictedShelterEventArmorStock;
                    case SHOP_CATEGORY_ITEMS:
                        return _gShopRestrictedShelterEventItemsStock;
                }
            case SHOP_STOCK_SHELTER_ALTERNATE_EVENT:
                switch ((u32)stockSelector >> 16) {
                    case SHOP_CATEGORY_WEAPONS:
                        return _gShopRestrictedShelterAlternateEventWeaponsStock;
                    case SHOP_CATEGORY_AMMUNITION:
                        return _gShopRestrictedShelterAlternateEventAmmunitionStock;
                    case SHOP_CATEGORY_ARMOR:
                        return _gShopRestrictedShelterAlternateEventArmorStock;
                    case SHOP_CATEGORY_ITEMS:
                        return _gShopRestrictedShelterAlternateEventItemsStock;
                }
            case SHOP_STOCK_DRYFIELD:
                switch ((u32)stockSelector >> 16) {
                    case SHOP_CATEGORY_WEAPONS:
                        return _gShopRestrictedDryfieldWeaponsStock;
                    case SHOP_CATEGORY_AMMUNITION:
                        return _gShopRestrictedDryfieldAmmunitionStock;
                    case SHOP_CATEGORY_ARMOR:
                        return _gShopRestrictedDryfieldArmorStock;
                    case SHOP_CATEGORY_ITEMS:
                        return _gShopRestrictedDryfieldItemsStock;
                }
                break;
            case SHOP_STOCK_DRYFIELD_LATE:
                switch ((u32)stockSelector >> 16) {
                    case SHOP_CATEGORY_WEAPONS:
                        return _gShopRestrictedDryfieldLateWeaponsStock;
                    case SHOP_CATEGORY_AMMUNITION:
                        return _gShopRestrictedDryfieldLateAmmunitionStock;
                    case SHOP_CATEGORY_ARMOR:
                        return _gShopRestrictedDryfieldLateArmorStock;
                    case SHOP_CATEGORY_ITEMS:
                        return _gShopRestrictedDryfieldLateItemsStock;
                }
                break;
            case SHOP_STOCK_ARMORY:
                switch ((u32)stockSelector >> 16) {
                    case SHOP_CATEGORY_WEAPONS:
                        return _gShopRestrictedArmoryWeaponsStock;
                    case SHOP_CATEGORY_AMMUNITION:
                        return _gShopRestrictedArmoryAmmunitionStock;
                    case SHOP_CATEGORY_ARMOR:
                        return _gShopRestrictedArmoryArmorStock;
                    case SHOP_CATEGORY_ITEMS:
                        return _gShopRestrictedArmoryItemsStock;
                }
                break;
            default:
                switch ((u32)stockSelector >> 16) {
                    case SHOP_CATEGORY_WEAPONS:
                        return _gShopRestrictedMistWeaponsStock;
                    case SHOP_CATEGORY_AMMUNITION:
                        return _gShopRestrictedMistAmmunitionStock;
                    case SHOP_CATEGORY_ARMOR:
                        return _gShopRestrictedMistArmorStock;
                    case SHOP_CATEGORY_ITEMS:
                        return _gShopRestrictedMistItemsStock;
                }
                break;
        }
    }
    return _gShopEmptyStock;
}

/// Draws an offered item or recharge-service row and opens its purchase/detail panel.
///
/// Borrows the owner's live item-list work and the current row in 0..itemCount-1;
/// current stock/unlock combinations offer at most 33 of its 64 row slots.
/// An active row publishes its row id for purchase. Recharge is unavailable
/// without carried weapon supply; catalog items are dimmed at their ownership
/// limit. Confirm opens the appropriate purchase panel; Triangle opens details.
/// Requires loaded item text/catalog and frame GPU storage. Dialog tasks own
/// their storage; allocation failure leaves catalog-row input active.
static void _shopDrawItemRow(UiList* list, UiObject* object)
{
    enum {
        SHOP_ROW_SUSPENDED_CONTROL_SHIFT = 16,
        SHOP_ROW_DIALOG_DELAY_TICKS      = 1,
        SHOP_ROW_DETAIL_TEMPLATE         = 45,
        SHOP_UNREACHED_ROW_BLOCKING_ITEM = 0x8F
    };
    TextDrawReq         textRequest;
    u8                  priceText[0x20];
    _ShopItemListWork*  work;
    InventoryItemRange* carriedItems;
    s32                 captionBaseY;
    UiObject*           specialRowDialog;
    UiObject*           purchaseDialog;
    s32                 purchaseBlocked;
    s32                 panelControl;
    s32                 rowId;
    s32                 unitPrice;

    work            = object->owner->work;
    purchaseBlocked = 0;
    rowId           = work->rowIds[list->currentItemIndex];
    carriedItems    = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        _gShopPreviewRowId = rowId;
    }

    if (rowId == SHOP_ROW_RECHARGE_SERVICE) {
        panelControl = object->panel.control.word;
        if (((panelControl >> SHOP_ROW_SUSPENDED_CONTROL_SHIFT) == USER_INTERFACE_PANEL_ACTIVE) || (panelControl == USER_INTERFACE_PANEL_ACTIVE)) {
            if (list->selectedItemIndex == list->currentItemIndex) {
                uiSetPromptText(_gShopRechargeServicePrompt, 0, 0);
            }
        }
        if (equipmentHasCarriedWeaponSupply() == 0) {
            list->colorRgb        = uiGetTextColor(object, USER_INTERFACE_TEXT_COLOR_DIMMED);
            list->rowInputEnabled = USER_INTERFACE_LIST_ROW_INACTIVE;
        }
        textRequest.x          = object->panel.contentOriginX.unsignedValue + list->rowTextX.signedValue;
        captionBaseY           = object->panel.contentOriginY.unsignedValue - 4;
        textRequest.y          = list->rowTextY.signedValue + captionBaseY;
        textRequest.otIndex    = object->panel.otIndex.signedValue + 1;
        textRequest.colorRgb   = list->colorRgb;
        textRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        textRequest.alignment  = TEXT_ALIGNMENT_LEFT;
        textRequest.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&textRequest, _gShopRechargeServiceLabel);
        if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadConfirmButtonMask) != 0) {
            sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
            uiSpawnObject(&_gShopWeaponSupplyRefillDescriptor, 0, USER_INTERFACE_PANEL_ACTIVE, SHOP_ROW_DIALOG_DELAY_TICKS, object);
            object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        }
        return;
    }

    // No current stock, tier or PE unlock produces 0xFFFC; its role is unproven.
    // Retain the dormant purchase path, including its out-of-catalog payload.
    if (rowId == 0xFFFC) {
        panelControl = object->panel.control.word;
        if (((panelControl >> SHOP_ROW_SUSPENDED_CONTROL_SHIFT) == USER_INTERFACE_PANEL_ACTIVE) || (panelControl == USER_INTERFACE_PANEL_ACTIVE)) {
            if (list->selectedItemIndex == list->currentItemIndex) {
                uiSetPromptText(gItemMenuEmptyPromptText, 0, 0);
            }
        }
        if (inventoryGetItemQuantity(carriedItems, SHOP_UNREACHED_ROW_BLOCKING_ITEM) != 0) {
            purchaseBlocked = 1;
            list->colorRgb  = uiGetTextColor(object, USER_INTERFACE_TEXT_COLOR_DIMMED);
        }
        textDrawUiLine(object, list->rowTextX.signedValue, list->rowTextY.signedValue, _gShopUnusedSpecialRowLabel, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
        if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE && purchaseBlocked == 0 && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadConfirmButtonMask) != 0) {
            sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
            specialRowDialog = uiSpawnObject(&_gShopPurchasePromptDescriptor, rowId, USER_INTERFACE_PANEL_ACTIVE, SHOP_ROW_DIALOG_DELAY_TICKS, object);
            if (specialRowDialog != NULL) {
                uiPositionRowDialog(&(specialRowDialog)->panel, list, &(object)->panel);
                object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            }
        }
        return;
    }

    unitPrice = gItemDescriptors[rowId].price;
    if (inventoryIsItemLimitReached(rowId) != 0) {
        purchaseBlocked = 1;
        list->colorRgb  = uiGetTextColor(object, USER_INTERFACE_TEXT_COLOR_DIMMED);
    }
    if (list->actionResult != USER_INTERFACE_LIST_ACTION_SKIP_ROW) {
        panelControl = object->panel.control.word;
        if (((panelControl >> SHOP_ROW_SUSPENDED_CONTROL_SHIFT) == USER_INTERFACE_PANEL_ACTIVE) || (panelControl == USER_INTERFACE_PANEL_ACTIVE)) {
            if (list->selectedItemIndex == list->currentItemIndex) {
                itemMenuSetItemDescriptionPrompt(rowId);
                itemMenuSetPreviewItem(rowId, CD_COMMAND_DISPLAY_LOAD_MENU);
            }
        }
    }
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (purchaseBlocked == 0 && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadConfirmButtonMask) != 0) {
            purchaseDialog = uiSpawnObject(&_gShopPurchasePromptDescriptor, rowId, USER_INTERFACE_PANEL_ACTIVE, SHOP_ROW_DIALOG_DELAY_TICKS, object);
            if (purchaseDialog != NULL) {
                sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
                uiPositionRowDialog(&(purchaseDialog)->panel, list, &(object)->panel);
                object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_TRIANGLE) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            uiSpawnObject(&gItemMenuPanelDescriptors[SHOP_ROW_DETAIL_TEMPLATE], rowId, USER_INTERFACE_PANEL_ACTIVE, SHOP_ROW_DIALOG_DELAY_TICKS, object);
            object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        }
    }
    itemMenuDrawItemRow(object, list->rowTextX.signedValue, list->rowTextY.signedValue, rowId, list->colorRgb, 0);
    if ((u32)(rowId - INVENTORY_CONSUMABLE_ITEM_FIRST) < INVENTORY_CONSUMABLE_ITEM_COUNT) {
        itemMenuDrawQuantity(object, list->rowTextX.signedValue, list->rowTextY.signedValue, _inventoryGetConsumableStackInfo(rowId)->packQty, list->colorRgb);
    }
    textItoaUnsigned(priceText, unitPrice);
    textDrawUiLine(object, -list->rowTextX.signedValue, list->rowTextY.signedValue, priceText, list->colorRgb, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
}

/// Appends an offered row or upgrades the first lower PE level of its ability.
///
/// `list` must be the list in the owning task's `_ShopItemListWork`; `rowId`
/// is a catalog id or `SHOP_ROW_RECHARGE_SERVICE`. Exact duplicates return
/// immediately. A higher PE level replaces the first lower level, but a lower
/// level arriving after a higher one can append a second row of that ability.
/// M.I.S.T. excludes the MP5A5 variants, Grenade Pistol and Tactical Vest only
/// while scanning an existing row, so an empty list bypasses that exclusion.
/// Newly appended catalog items are identified. The stock builder proves
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

/// Sorts offered row ids by ascending inventory catalog key.
///
/// Borrows the work's writable row ids and read-only list metadata; itemCount
/// must be in 0..`ARRAY_SIZE(work->rowIds)`. Empty and singleton lists need no
/// swaps. Only ids move: no prices or list selection are changed. Swapping
/// distant rows can change the original order of equal keys.
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
                    for (tierItemIndex = 0; tierItemIndex < ARRAY_SIZE(_gShopReplayTierLadder[tierIndex].items); tierItemIndex++) {
                        itemId = _gShopReplayTierLadder[tierIndex].items[tierItemIndex];
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
    _gShopPreviewRowId = SHOP_PREVIEW_NONE;
}

/// NUL-terminated title of the offered-item list panel.
static const char _gShopItemListTitle[] = "Select";

/// Byte-text currency label for item prices, the BP balance and purchase totals.
static const u8 _gShopBpLabel[] = "BP";

/// NUL-terminated title of the category-and-service menu.
static const char _gShopCategoryListTitle[] = "List";

/// Byte-text heading for occupied carried inventory rows over row capacity.
static const u8 _gShopCarriedRowCapacityLabel[] = "TOTAL";

/// NUL-terminated title of the shop's purchase-error notices.
static const char _gShopNoticeTitle[] = "Notice";

STATIC_ASSERT(sizeof(SHOP_WEAPON_SUPPLY_REFILL_TITLE_TRAILING_BYTES) == 2, shopWeaponSupplyRefillTitleTrailingBytes);

/// NUL-terminated title of the built-in weapon-supply refill panel.
///
/// The eight stored bytes hold "Charge", its NUL and one retained byte supplied
/// by `SHOP_WEAPON_SUPPLY_REFILL_TITLE_TRAILING_BYTES`. Label drawing stops at the
/// NUL; the string literal's final terminator is excluded from the array.
static const char _gShopWeaponSupplyRefillTitle[8] = "Charge\0" SHOP_WEAPON_SUPPLY_REFILL_TITLE_TRAILING_BYTES;

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
    uiDrawPanelLabel(&object->panel, _gShopItemListTitle);
    if (task->state == SHOP_PANEL_INITIAL) {
        allocation = memCalloc(sizeof(_ShopItemListWork), 0);
        if (allocation != NULL) {
            work                      = allocation;
            task->work                = work;
            work->list.rowCallbacks   = _gShopItemRowCallbacks;
            work->list.wrapNavigation = 0;
            work->list.rowHeight      = 0xF;
            _shopBuildItemList(&work->list, object);
            uiFitPanelToList(&work->list, &object->panel);
            work->list.flags = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
            uiSetListSystemCursorSound(&work->list, 1);
            object->panel.bounds.unsignedRect.h += 8;
            work->list.topInset                  = 8;
            uiSpawnObject(&_gShopItemPreviewDescriptor, 0, USER_INTERFACE_PANEL_INACTIVE, 0, object);
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
    textDrawString(&req, _gShopBpLabel);

    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadMenuButtonMask) != 0) {
            object->result = USER_INTERFACE_RESULT_CANCEL;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadCancelButtonMask) != 0) {
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
        textDrawUiLine(object, list->rowTextX.signedValue, list->rowTextY.signedValue, _gShopPassLabel, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
        if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadConfirmButtonMask) != 0) {
            object->result = USER_INTERFACE_RESULT_CONFIRM;
        }
        return;
    }

    label                          = _gShopWeaponsCategoryLabel;
    object->owner->spawnArg1.value = (u16)object->owner->spawnArg1.value;
    switch (list->currentItemIndex) {
        case SHOP_CATEGORY_WEAPONS:
            break;
        case SHOP_CATEGORY_AMMUNITION:
            label                           = _gShopAmmunitionCategoryLabel;
            object->owner->spawnArg1.value |= (SHOP_CATEGORY_AMMUNITION << 16);
            break;
        case SHOP_CATEGORY_ARMOR:
            label                           = _gShopArmorCategoryLabel;
            object->owner->spawnArg1.value |= (SHOP_CATEGORY_ARMOR << 16);
            break;
        case SHOP_CATEGORY_ITEMS:
            label                           = _gShopItemsCategoryLabel;
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
            uiSetPromptText(gItemMenuEmptyPromptText, 0, 0);
        }
    }

    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadConfirmButtonMask) != 0) {
        sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
        uiSpawnObject(&_gShopItemListDescriptor, object->owner->spawnArg1, USER_INTERFACE_PANEL_ACTIVE, 1, object);
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
    list           = &_gShopCategoryList;
    object->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&object->panel, _gShopCategoryListTitle);
    if (task->state == SHOP_PANEL_INITIAL) {
        itemMenuClearPreviewItems();
        gItemMenuInfoPanel = NULL;
        uiSpawnObject(&_gShopBalancePanelDescriptor, task->spawnArg1, USER_INTERFACE_PANEL_INACTIVE, 1, object);
        uiSpawnObject(&gItemMenuTransferPanelDescriptors[ITEM_MENU_TRANSFER_PROMPT_PANEL], 0, USER_INTERFACE_PANEL_INACTIVE, 0, object);
        list->itemCount                     = SHOP_CATEGORY_COUNT + 1;
        list->visibleRowCount.unsignedValue = SHOP_CATEGORY_COUNT + 1;
        uiFitPanelToList(list, &object->panel);
        list->flags = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        uiSetListSystemCursorSound(list, 1);
        task->state += 1;
    }
    uiUpdateList(list, &object->panel);
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadCancelButtonMask | gPadMenuButtonMask) != 0) {
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
                gUiPromptObject = NULL;
                object->result  = childResult;
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
    textDrawString(&balanceLabelRequest, _gShopBpLabel);

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
    textDrawString(&capacityLabelRequest, _gShopCarriedRowCapacityLabel);

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
    textDrawString(&request, _gShopPurchaseLabel);

    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadConfirmButtonMask) != 0) {
        playerStatus = &gPlayerStatus;
        packPrice    = gItemDescriptors[itemId].price;
        carriedItems = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
        sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
        if (playerStatus->bp >= packPrice) {
            if (inventoryCanAddItem(carriedItems, itemId) == 0) {
                if ((u32)(itemId - INVENTORY_CONSUMABLE_ITEM_FIRST) < INVENTORY_CONSUMABLE_ITEM_COUNT && inventoryGetItemQuantity(carriedItems, itemId) != 0) {
                    uiSpawnObject(&_gShopNoticeDescriptor, SHOP_NOTICE_AMMUNITION_CAPACITY, USER_INTERFACE_PANEL_ACTIVE, 1, object);
                } else {
                    uiSpawnObject(&_gShopNoticeDescriptor, SHOP_NOTICE_INVENTORY_FULL, USER_INTERFACE_PANEL_ACTIVE, 1, object);
                }
                object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            } else if ((object->owner->parent->spawnArg1.value >> 16) == SHOP_CATEGORY_AMMUNITION) {
                quantityObject = uiSpawnObject(&_gShopPurchaseQuantityDescriptor, itemId, USER_INTERFACE_PANEL_ACTIVE, 1, object);
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
            uiSpawnObject(&_gShopNoticeDescriptor, SHOP_NOTICE_INSUFFICIENT_BP, USER_INTERFACE_PANEL_ACTIVE, 1, object);
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
            message = _gShopInventoryFullText;
            break;
        case SHOP_NOTICE_AMMUNITION_CAPACITY:
            message = _gShopAmmunitionCapacityText;
            break;
        default:
            message = _gShopInsufficientBpText;
            break;
    }

    uiDrawPanelLabel(&object->panel, _gShopNoticeTitle);
    object->result = USER_INTERFACE_RESULT_NONE;
    if (task->state == SHOP_PANEL_INITIAL) {
        uiSizePanelForTextDefault(&object->panel, message);
        task->killCountdown = SHOP_NOTICE_DURATION_TICKS;
        task->state        += 1;
    }
    textDrawUiLines(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 0xF, message, SHOP_TEXT_COLOR_RGB, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    task->killCountdown -= gDisplayState.frameTicks;
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadMenuButtonMask) != 0) {
            object->result = USER_INTERFACE_RESULT_CANCEL;
            return;
        }
        if (task->killCountdown <= 0 || padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadConfirmButtonMask | gPadCancelButtonMask) != 0) {
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
    uiDrawPanelLabel(&object->panel, _gShopWeaponSupplyRefillTitle);

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
            SHOP_REFILL_SUPPLY = supply;
            refillWeaponItemId = supply->weaponItemId;
            weaponLoad         = equipmentGetWeaponLoad(refillWeaponItemId);
            if (SHOP_REFILL_SUPPLY->supplyLoad == EQUIPMENT_WEAPON_SUPPLY_PRIMARY) {
                SHOP_REFILL_DISPLAY_QUANTITY = weaponLoad->primaryQty;
                weaponLoad->primaryQty       = equipmentGetWeaponLoadCapacity(refillWeaponItemId, EQUIPMENT_WEAPON_SUPPLY_PRIMARY);
            } else {
                SHOP_REFILL_DISPLAY_QUANTITY = weaponLoad->secondaryQty;
                weaponLoad->secondaryQty     = equipmentGetWeaponLoadCapacity(refillWeaponItemId, EQUIPMENT_WEAPON_SUPPLY_SECONDARY);
            }
            task->killCountdown            = SHOP_REFILL_DURATION_FRAMES;
            SHOP_REFILL_DISPLAY_QUANTITY <<= SHOP_REFILL_FRACTION_BITS;
            task->state                    = task->state + 1;
        }
    }

    // Keep drawing the last selected supply during completion and closure.
    weaponItemId = SHOP_REFILL_SUPPLY->weaponItemId;
    supplyItemId = SHOP_REFILL_SUPPLY->supplyItemId;
    if (SHOP_REFILL_SUPPLY->supplyLoad == EQUIPMENT_WEAPON_SUPPLY_PRIMARY) {
        capacityFixed = equipmentGetWeaponLoadCapacity(weaponItemId, EQUIPMENT_WEAPON_SUPPLY_PRIMARY);
    } else {
        capacityFixed = equipmentGetWeaponLoadCapacity(weaponItemId, EQUIPMENT_WEAPON_SUPPLY_SECONDARY);
    }
    capacityFixed               <<= SHOP_REFILL_FRACTION_BITS;
    SHOP_REFILL_DISPLAY_QUANTITY += SHOP_REFILL_STEP_FIXED;
    if (capacityFixed < SHOP_REFILL_DISPLAY_QUANTITY) {
        SHOP_REFILL_DISPLAY_QUANTITY = capacityFixed;
    }

    contentTop = object->panel.contentTop.signedValue;
    itemMenuDrawItemRow(object, object->panel.contentLeft.signedValue + 2, contentTop + 0xF, weaponItemId, SHOP_TEXT_COLOR_RGB, 0);
    uiDrawHorizontalSeparator(&object->panel, object->panel.contentLeft.signedValue, object->panel.contentRight.signedValue, contentTop + 0x12);
    itemMenuDrawItemRow(object, object->panel.contentLeft.signedValue + 2, contentTop + 0x23, supplyItemId, SHOP_TEXT_COLOR_RGB, 0);
    itemMenuDrawQuantity(object, object->panel.contentLeft.signedValue + 2, contentTop + 0x23, SHOP_REFILL_DISPLAY_QUANTITY >> SHOP_REFILL_FRACTION_BITS, SHOP_TEXT_COLOR_RGB);
    contentBottom = object->panel.contentBottom.signedValue;
    itemMenuDrawMeter(&object->panel, object->panel.contentLeft.signedValue + 2, object->panel.contentRight.signedValue - 2, contentBottom - 6, capacityFixed,
                      SHOP_REFILL_DISPLAY_QUANTITY, SHOP_REFILL_METER_COLOR_RGB);

    if (task->state == SHOP_REFILL_ANIMATE) {
        remainingFrames     = task->killCountdown - 1;
        task->killCountdown = remainingFrames;
        panelControl        = object->panel.control.word;
        if (panelControl == USER_INTERFACE_PANEL_ACTIVE && (remainingFrames <= 0 || padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadCancelButtonMask | gPadConfirmButtonMask) != 0)) {
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

    rowId        = _gShopPreviewRowId;
    object       = task->spawnArg2.pointer;
    task->status = 0;
    if (cdCmdIsIdle() && _gShopPreviewRowId == itemMenuGetPrimaryPreviewItem()) {
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
        textDrawString(&amountRequest, _gShopCarriedAmountLabel);
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
/// the selected pack count, starting at one. The catalog price must be
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
    packPrice = gItemDescriptors[itemId].price;

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
    textDrawUiLine(object, contentLeft + 0x98, lineY, _gShopPackMultiplierText, SHOP_TEXT_COLOR_RGB, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
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
    textDrawString(&priceLabelRequest, _gShopBpLabel);

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
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadConfirmButtonMask) != 0) {
            gPlayerStatus.bp -= packPrice * task->extraState.value;
            for (packIndex = 0; packIndex < task->extraState.value; packIndex++) {
                inventoryGiveItem(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, itemId, INVENTORY_GIVE_ONE_PACK);
            }
            sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
            parentObject->result = USER_INTERFACE_RESULT_CONFIRM;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadCancelButtonMask) != 0) {
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
    textDrawString(&request, _gShopPassLabel);

    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadConfirmButtonMask) != 0) {
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

    list           = &_gShopPurchasePromptList;
    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    if (task->state == SHOP_PANEL_INITIAL) {
        uiFitPanelToList(list, &object->panel);
        uiSetListSystemCursorSound(list, 1);
        task->state += 1;
    }
    uiUpdateList(list, &object->panel);
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadCancelButtonMask) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CANCEL, 0, 0);
            object->result = USER_INTERFACE_RESULT_CONFIRM;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadMenuButtonMask) != 0) {
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
        object = uiSpawnObject(&_gShopCategoryListDescriptor, task->spawnArg1, USER_INTERFACE_PANEL_ACTIVE, 1, NULL);
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
