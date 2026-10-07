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

UiListRowCallback D_8010D630[1] = { Gp_ItemMoveRow };

UiList Gp_InvLists[2] = {
    { D_8010D630, 60, { 60 }, 0, 15, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 },
    { D_8010D630, 60, { 60 }, 0, 15, 0, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0 },
};

UiListRowCallback Gp_ItemActionFns[3] = {
    func_800BD6DC,
    Gp_ItemActionConfirm,
    NULL,
};

s32 Gp_CanMoveItems(void)
{
    InventoryItemRange* src;
    InventoryItemRow*   table;
    s32                 row;
    s32                 count;
    s32                 blocked;
    s32                 ret;
    s32                 i;

    src     = &Gp_MoveScanSrc;
    ret     = 0;
    table   = inventoryGetRangeTable(src);
    row     = src->firstRow;
    count   = inventoryCountOccupiedRows(src + 1);
    blocked = 0; /* nothing sets it, yet the original still tests it */
    if (inventoryCountOccupiedRows(src) > 0) {
        for (i = 0; i < src->rowCount; i++, row++) {
            if (table[row].itemId != INVENTORY_ITEM_NONE) {
                /* 0xA0-0xBF items need no new row if the destination already holds one */
                if ((u8)(table[row].itemId + 0x60) < 0x20) {
                    if (inventoryFindLastItemRowInRange(table[row].itemId, &Gp_MoveScanDst) == 0) {
                        count++;
                    }
                } else {
                    count++;
                }
            }
        }
        if (blocked == 0 && Gp_MoveScanDst.rowCount >= count) {
            ret = 1;
        }
    }
    return ret;
}
