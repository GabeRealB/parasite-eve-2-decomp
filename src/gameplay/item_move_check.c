#include "item_menu.h"

#include "types.h"

#include "item_use.h"
#include "gameplay/items.h"

#include "main/mc_types.h"
#include "main/ui_types.h"

extern UiListRowCallback D_8010D630[1];

u8 Gp_StrMove2[] = "Move";

char Gp_StrSwitch[] = "Switch";

char Gp_StrSetAmmoHelp[] = "Set ammo# with the directional\nbuttons. } button to confirm.";

char Gp_StrAmmoLocked[] = "Ammo loaded in a weapon\ncannot be moved.";

char Gp_StrMaxCapacity[] = "Maximum capacity.";

u8 Gp_StrAll[] = "All";

u8 Gp_StrSelect[] = "Select";

u8 Gp_StrEnd[] = "End";

u8 Gp_StrDiscard[] = "Discard";

InventoryItemRange Gp_MoveScanSrc = { 0, 60, INVENTORY_ITEM_TABLE_INDIRECT, 0 };

InventoryItemRange Gp_MoveScanDst = { 60, 60, INVENTORY_ITEM_TABLE_INDIRECT, 0 };

UiListRowCallback D_8010D630[1] = { itemMenuDrawTransferInventoryRow };

UiList Gp_InvLists[2] = {
    { D_8010D630, 60, { 60 }, 0, 15, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 },
    { D_8010D630, 60, { 60 }, 0, 15, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 },
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

    sourceRange  = &Gp_MoveScanSrc;
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
                    if (inventoryFindLastItemRowInRange(sourceRows[sourceRow].itemId, &Gp_MoveScanDst) == NULL) {
                        requiredRows++;
                    }
                } else {
                    requiredRows++;
                }
            }
        }
        if (alwaysZero == 0 && Gp_MoveScanDst.rowCount >= requiredRows) {
            canMoveAll = 1;
        }
    }
    return canMoveAll;
}
