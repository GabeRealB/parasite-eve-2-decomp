#include "item_menu.h"

#include "types.h"

#include "item_use.h"
#include "gameplay/items.h"

#include "main/mc_types.h"
#include "main/ui_types.h"

u8 Gp_StrMove2[] = "Move";

char Gp_StrSwitch[] = "Switch";

char Gp_StrSetAmmoHelp[] = "Set ammo# with the directional\nbuttons. } button to confirm.";

char Gp_StrAmmoLocked[] = "Ammo loaded in a weapon\ncannot be moved.";

char Gp_StrMaxCapacity[] = "Maximum capacity.";

u8 gItemMenuTransferAllText[] = "All";

u8 gItemMenuTransferSelectText[] = "Select";

u8 gItemMenuTransferEndText[] = "End";

u8 gItemMenuTransferDiscardText[] = "Discard";

InventoryItemRange gItemMenuTransferRanges[ITEM_MENU_PANE_COUNT] = {
    { .firstRow = 0, .rowCount = 60, .tableId = INVENTORY_ITEM_TABLE_INDIRECT },
    { .firstRow = 60, .rowCount = 60, .tableId = INVENTORY_ITEM_TABLE_INDIRECT },
};

/// Single inventory-row callback shared by the container and carried-item pane lists.
///
/// Lists must enable USER_INTERFACE_LIST_SHARED_ROW_CALLBACK before dispatch,
/// so entry zero handles every row without indexing beyond this one-element table.
static UiListRowCallback _gItemMenuTransferInventoryRowCallbacks[1] = { itemMenuDrawTransferInventoryRow };

UiList Gp_InvLists[2] = {
    { _gItemMenuTransferInventoryRowCallbacks, 60, { 60 }, 0, 15, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 },
    { _gItemMenuTransferInventoryRowCallbacks, 60, { 60 }, 0, 15, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 },
};

UiListRowCallback Gp_ItemActionFns[3] = {
    itemMenuDrawTransferMoveRow,
    itemMenuDrawSwitchRow,
    NULL,
};

s32 itemMenuCanMoveAllItems(void)
{
    const InventoryItemRange* sourceRange;
    const InventoryItemRow*   sourceRows;
    s32                       sourceRow;
    s32                       requiredRows;
    s32                       alwaysZero;
    s32                       canMoveAll;
    s32                       rowOffset;

    sourceRange  = gItemMenuTransferRanges;
    canMoveAll   = 0;
    sourceRows   = inventoryGetRangeTable(sourceRange);
    sourceRow    = sourceRange->firstRow;
    requiredRows = inventoryCountOccupiedRows(sourceRange + 1);
    // The binary retains this zero-valued guard across the inventory calls.
    alwaysZero = 0;
    if (inventoryCountOccupiedRows(sourceRange) > 0) {
        // Existing destination consumables reuse their stack's row.
        for (rowOffset = 0; rowOffset < sourceRange->rowCount; rowOffset++, sourceRow++) {
            if (sourceRows[sourceRow].itemId != INVENTORY_ITEM_NONE) {
                if ((u8)(sourceRows[sourceRow].itemId - INVENTORY_CONSUMABLE_ITEM_FIRST) < INVENTORY_CONSUMABLE_ITEM_COUNT) {
                    if (inventoryFindLastItemRowInRange(sourceRows[sourceRow].itemId, &gItemMenuTransferRanges[ITEM_MENU_PANE_CARRIED]) == NULL) {
                        requiredRows++;
                    }
                } else {
                    requiredRows++;
                }
            }
        }
        if (alwaysZero == 0 && gItemMenuTransferRanges[ITEM_MENU_PANE_CARRIED].rowCount >= requiredRows) {
            canMoveAll = 1;
        }
    }
    return canMoveAll;
}
