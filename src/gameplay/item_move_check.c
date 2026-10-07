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

/* Count item `id` in saved rows 0..254 through a cleared range. */
#define GP_TOTAL_QTY(scan, id) (memset(&(scan), 0, sizeof(scan)), (scan).rowCount = INVENTORY_ITEM_RANGE_MAX_ROWS, Gp_SumScanQty(&(scan), (id)))

/* Item names and descriptions shared by the inventory tables. */

/* Gives `scan` one `weapon` and loads it with `ammo`. */
#define GP_GIVE_LOADED(scan, weapon, ammo)           \
    do {                                             \
        inventoryGiveItem(scan, weapon, 1);          \
        Gp_EquipRelatedItem(scan, weapon, ammo, -1); \
    } while (0)

/* Clears the carried inventory, equips the starting armour, restores HP/MP,
 * and gives the initial supplies and their attachment slots. */
#define _gpInitStartingItems(scan, cfg)                      \
    do {                                                     \
        inventoryClearItems(scan);                           \
        inventoryGiveItem(scan, 0x60, 1);                    \
        Gp_EquipMod(0x60);                                   \
        (cfg)->hp = (cfg)->hpMax;                            \
        (cfg)->mp = (cfg)->mpMax;                            \
        inventoryGiveItem(scan, 0x92, 1);                    \
        inventoryGiveItem(scan, 0x40, 1)->attachSlot    = 1; \
        inventoryGiveItem(scan, 0xA0, 0x64)->attachSlot = 2; \
    } while (0)

/* Item table a scan window lies in. */

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
    count   = Gp_CountScanItems(src + 1);
    blocked = 0; /* nothing sets it, yet the original still tests it */
    if (Gp_CountScanItems(src) > 0) {
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
