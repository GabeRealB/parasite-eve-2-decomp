#include "gameplay/items.h"

#include "types.h"

#include "attachments.h"
#include "gameplay/inventory.h"
#include "inventory.h"
#include "items.h"

#include "debug/nmc_names.h"

#include "main/mc.h"
#include "main/session.h"
#include "main/task_types.h"
#include "main/wipsys.h"

#include "rooms/acropolis_fire_escape.h"

#include "rooms/acropolis_square.h"

#include "rooms/dryfield_gas_station.h"

#include "rooms/dryfield_motel_lobby.h"

#include "rooms/dryfield_motel_room_6.h"

#include "rooms/dryfield_night_gas_station.h"

#include "rooms/dryfield_night_motel_lobby.h"

#include "rooms/dryfield_night_motel_room_6.h"

#include "rooms/dryfield_night_trailer_coach.h"

#include "rooms/dryfield_trailer_coach.h"

#include "rooms/mine_refuge.h"

#include "rooms/mist_parking.h"

#include "rooms/shelter_1f_tent.h"

#include "rooms/shelter_b1_sterilization_room.h"

#include "rooms/shelter_b1_underground_parking.h"

#include "rooms/shelter_b2_laboratory.h"

#include "rooms/shelter_b3_incinerator_control_room.h"

#include "rooms/shelter_b6_nursery.h"

#include "rooms/shelter_r47.h"

/// Row `index` elements after `rows`.
/// The result borrows `rows`; `index` must name a row in that table.
#define gpStackLimitAt(rows, index) (&(rows)[index])

static inline InventoryItemRow* _inventoryGetRangeTable(const InventoryItemRange* range);

/// True if `arg2` of item `arg1` can be added to the item table selected
/// by `arg0`. Ids `>= 0x100` always succeed. Ids `0xA0..0xFF` stack onto
/// an existing row when `qty + arg2` fits `Gp_StackLimits[id-0xA0].maxHeld`;
/// `arg2 < 0` uses that row's `packQty` as the addend. Other ids need a
/// free slot.
static s32 Gp_CanAddItemQty(InventoryItemRange* arg0, s32 arg1, s32 arg2);

static inline bool _itemIsIdentified(s32 itemId);

static inline void _equipmentClearRemovableLoads(s32 weaponItemId);

u8 Gp_ItemSortKey0[72] = {
    0,
    1,
    2,
    3,
    10,
    6,
    4,
    5,
    9,
    22,
    45,
    81,
    16,
    75,
    82,
    87,
    88,
    89,
    90,
    91,
    92,
    93,
    94,
    95,
    96,
    97,
    98,
    99,
    100,
    101,
    102,
    103,
    104,
    105,
    106,
    107,
    108,
    109,
    110,
    111,
    112,
    113,
    114,
    115,
    116,
    117,
    118,
    119,
    120,
    121,
    122,
    0,
    0,
    0,
    83,
    84,
    85,
    86,
    79,
    78,
    8,
    7,
    11,
    80,
    76,
    77,
    48,
    49,
    50,
    47,
    46,
    255
};
u8 Gp_ItemSortKey60[15] = {
    61,
    72,
    67,
    62,
    65,
    71,
    70,
    64,
    66,
    68,
    69,
    73,
    63,
    74,
    255
};
u8 Gp_ItemSortKey80[33] = {
    15,
    13,
    18,
    14,
    17,
    0,
    0,
    0,
    26,
    0,
    55,
    56,
    30,
    31,
    32,
    37,
    51,
    0,
    12,
    38,
    39,
    60,
    33,
    0,
    42,
    40,
    41,
    43,
    44,
    19,
    20,
    21,
    255
};
u8 Gp_ItemSortKeyA0[33] = {
    23,
    24,
    25,
    0,
    0,
    0,
    27,
    28,
    29,
    57,
    58,
    59,
    34,
    35,
    36,
    52,
    53,
    54,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    255
};

/* Count item `id` in saved rows 0..254 through a cleared range. */
#define GP_TOTAL_QTY(scan, id) (memset(&(scan), 0, sizeof(scan)), (scan).rowCount = INVENTORY_ITEM_RANGE_MAX_ROWS, Gp_SumScanQty(&(scan), (id)))

/* Item names and descriptions shared by the inventory tables. */

/* Gives `scan` one `weapon` and loads it with `ammo`. */
#define GP_GIVE_LOADED(scan, weapon, ammo)           \
    do {                                             \
        Gp_GiveItem(scan, weapon, 1);                \
        Gp_EquipRelatedItem(scan, weapon, ammo, -1); \
    } while (0)

/* Clears the carried inventory, equips the starting armour, restores HP/MP,
 * and gives the initial supplies and their attachment slots. */
#define _gpInitStartingItems(scan, cfg)                \
    do {                                               \
        Gp_ClearScanItems(scan);                       \
        Gp_GiveItem(scan, 0x60, 1);                    \
        Gp_EquipMod(0x60);                             \
        (cfg)->hp = (cfg)->hpMax;                      \
        (cfg)->mp = (cfg)->mpMax;                      \
        Gp_GiveItem(scan, 0x92, 1);                    \
        Gp_GiveItem(scan, 0x40, 1)->attachSlot    = 1; \
        Gp_GiveItem(scan, 0xA0, 0x64)->attachSlot = 2; \
    } while (0)

/// Borrows the complete row table selected by a range, before its first-row offset.
///
/// The range owns no rows. Unrecognized selectors use the live save table;
/// the indirect table must already be available when that selector is used.
static inline InventoryItemRow* _inventoryGetRangeTable(const InventoryItemRange* range)
{
    InventoryItemRow* table;

    switch (range->tableId) {
        case INVENTORY_ITEM_TABLE_AREA_GRANTS:
            table = Gp_ItemTable2;
            break;
        case INVENTORY_ITEM_TABLE_INDIRECT:
            table = Gp_ItemTable1;
            break;
        default:
            table = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemRows;
            break;
    }
    return table;
}

void inventorySortItems(const InventoryItemRange* range, s32 unused)
{
    enum {
        INVENTORY_SORT_ARMOR_ITEM_FIRST = 0x60,
        INVENTORY_SORT_CLASS_ID_COUNT   = 0x20,
        INVENTORY_SORT_EMPTY_KEY        = 0x1000,
        INVENTORY_SORT_FALLBACK_OFFSET  = 0x100
    };

    InventoryItemRow* table;
    InventoryItemRow* placedRow;
    InventoryItemRow* candidateRow;
    InventoryItemRow  swappedRow;
    s32               placedIndex;
    s32               candidateIndex;
    s32               sortKey;
    s32               lowestKey;
    s32               itemId;
    s32               rowCount;

    /// Computes catalogue order, with unmapped ids ordered numerically and empty rows last.
    ///
    /// `id` must be a side-effect-free scalar and `key` a distinct scalar lvalue.
    /// Reads `id` repeatedly and writes only `key`; its class index is local.
    /// This binding is available only within this function.
#define INVENTORY_GET_SORT_KEY(id, key)                                     \
    do {                                                                    \
        s32 classIndex;                                                     \
        (key) = 0;                                                          \
        if ((id) == INVENTORY_ITEM_NONE) {                                  \
            (key) = INVENTORY_SORT_EMPTY_KEY;                               \
        } else if ((u32)((id) - 1) < INVENTORY_SORT_ARMOR_ITEM_FIRST - 1) { \
            (key) = Gp_ItemSortKey0[(id)];                                  \
        } else {                                                            \
            classIndex = (id) - INVENTORY_SORT_ARMOR_ITEM_FIRST;            \
            if ((u32)classIndex < INVENTORY_SORT_CLASS_ID_COUNT) {          \
                (key) = Gp_ItemSortKey60[classIndex];                       \
            } else {                                                        \
                classIndex = (id) - EQUIPMENT_WEAPON_ITEM_FIRST;            \
                if ((u32)classIndex < INVENTORY_SORT_CLASS_ID_COUNT) {      \
                    (key) = Gp_ItemSortKey80[classIndex];                   \
                } else {                                                    \
                    classIndex = (id) - INVENTORY_CONSUMABLE_ITEM_FIRST;    \
                    if ((u32)classIndex < INVENTORY_SORT_CLASS_ID_COUNT) {  \
                        (key) = Gp_ItemSortKeyA0[classIndex];               \
                    }                                                       \
                }                                                           \
            }                                                               \
        }                                                                   \
        if ((key) == 0) {                                                   \
            (key) = (id) + INVENTORY_SORT_FALLBACK_OFFSET;                  \
        }                                                                   \
    } while (0)

    placedIndex = 0;
    if ((range->rowCount - 1) > 0) {
        do {
            table = _inventoryGetRangeTable(range);

            placedRow  = table + range->firstRow;
            placedRow += placedIndex;
            itemId     = placedRow->itemId;

            INVENTORY_GET_SORT_KEY(itemId, sortKey);
            lowestKey = sortKey;

            table = _inventoryGetRangeTable(range);

            candidateIndex = placedIndex + 1;
            // Exchange complete rows whenever a lower catalogue key is found.
            candidateRow  = table + range->firstRow;
            candidateRow += placedIndex + 1;
            if (candidateIndex < range->rowCount) {
                do {
                    itemId = candidateRow->itemId;

                    INVENTORY_GET_SORT_KEY(itemId, sortKey);
                    if (sortKey < lowestKey) {
                        lowestKey     = sortKey;
                        swappedRow    = *placedRow;
                        *placedRow    = *candidateRow;
                        *candidateRow = swappedRow;
                    }
                    rowCount = range->rowCount;
                    candidateIndex++;
                    candidateRow++;
                } while (candidateIndex < rowCount);
            }
            rowCount = range->rowCount;
            placedIndex++;
        } while (placedIndex < (rowCount - 1));
    }
#undef INVENTORY_GET_SORT_KEY
}

/// True if `arg2` of item `arg1` can be added to the item table selected
/// by `arg0`. Ids `>= 0x100` always succeed. Ids `0xA0..0xFF` stack onto
/// an existing row when `qty + arg2` fits `Gp_StackLimits[id-0xA0].maxHeld`;
/// `arg2 < 0` uses that row's `packQty` as the addend. Other ids need a
/// free slot.
static s32 Gp_CanAddItemQty(InventoryItemRange* arg0, s32 arg1, s32 arg2)
{
    InventoryItemRow*         tmp;
    InventoryItemRow*         table;
    InventoryItemRow*         rec;
    s32                       i;
    s32                       occupied;
    s32                       count;
    s32                       start;
    s32                       limit;
    s32                       used;
    s32                       found;
    InventoryItemRow*         table2;
    InventoryItemRow*         walker;
    s32                       count2;
    s32                       start2;
    InventoryConsumableStack* stacks;
    s32                       idx;
    InventoryConsumableStack* stack;
    s32                       capacity;

    switch (arg0->tableId) {
        case INVENTORY_ITEM_TABLE_AREA_GRANTS:
            tmp = Gp_ItemTable2;
            break;
        case INVENTORY_ITEM_TABLE_INDIRECT:
            tmp = Gp_ItemTable1;
            break;
        default:
            tmp = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemRows;
            break;
    }
    table    = tmp;
    i        = 0;
    count    = arg0->rowCount;
    start    = arg0->firstRow;
    occupied = i;
    if (count != 0) {
        limit = count;
        rec   = gpItemRowAt(table, start);
        do {
            if (rec->itemId != INVENTORY_ITEM_NONE) {
                occupied++;
            }
            i++;
            rec++;
        } while (i < limit);
    }

    capacity = arg0->rowCount;
    used     = occupied;
    if (arg1 >= 0x100) {
        return 1;
    }

    if (arg1 >= 0xA0) {
        found  = 0;
        start2 = arg0->firstRow;
        switch (arg0->tableId) {
            case INVENTORY_ITEM_TABLE_AREA_GRANTS:
                table2 = Gp_ItemTable2;
                break;
            case INVENTORY_ITEM_TABLE_INDIRECT:
                table2 = Gp_ItemTable1;
                break;
            default:
                table2 = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemRows;
                break;
        }
        if (arg2 < 0) {
            arg2 = Gp_StackLimits[arg1 - 0xA0].packQty;
        }
        i      = 0;
        count2 = arg0->rowCount;
        if (count2 != 0) {
            stacks = Gp_StackLimits;
            idx    = arg1 - 0xA0;
            stack  = gpStackLimitAt(stacks, idx);
            walker = gpItemRowAt(table2, start2);
            do {
                if (walker->itemId == arg1) {
                    found = 2;
                    if (stack->maxHeld >= walker->qty + arg2) {
                        found = 1;
                    }
                    break;
                }
                i++;
                walker++;
            } while (i < count2);
        }

        if (found == 0) {
            used++;
        }
        if (found == 2) {
            return 0;
        }
    } else {
        used++;
    }
    return used <= capacity;
}

s32 Gp_CanAddItem(InventoryItemRange* arg0, s32 arg1)
{
    InventoryItemRow*         tmp;
    InventoryItemRow*         table;
    InventoryItemRow*         rec;
    s32                       i;
    s32                       occupied;
    s32                       count;
    s32                       start;
    s32                       limit;
    s32                       used;
    s32                       found;
    InventoryItemRow*         table2;
    InventoryItemRow*         walker;
    s32                       count2;
    s32                       start2;
    InventoryConsumableStack* stacks;
    s32                       idx;
    InventoryConsumableStack* stack;
    s32                       capacity;

    switch (arg0->tableId) {
        case INVENTORY_ITEM_TABLE_AREA_GRANTS:
            tmp = Gp_ItemTable2;
            break;
        case INVENTORY_ITEM_TABLE_INDIRECT:
            tmp = Gp_ItemTable1;
            break;
        default:
            tmp = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemRows;
            break;
    }
    table    = tmp;
    i        = 0;
    count    = arg0->rowCount;
    start    = arg0->firstRow;
    occupied = i;
    if (count != 0) {
        limit = count;
        rec   = gpItemRowAt(table, start);
        do {
            if (rec->itemId != INVENTORY_ITEM_NONE) {
                occupied++;
            }
            i++;
            rec++;
        } while (i < limit);
    }

    capacity = arg0->rowCount;
    used     = occupied;
    if (arg1 >= 0x100) {
        return 1;
    }

    if (arg1 >= 0xA0) {
        found  = 0;
        start2 = arg0->firstRow;
        switch (arg0->tableId) {
            case INVENTORY_ITEM_TABLE_AREA_GRANTS:
                table2 = Gp_ItemTable2;
                break;
            case INVENTORY_ITEM_TABLE_INDIRECT:
                table2 = Gp_ItemTable1;
                break;
            default:
                table2 = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemRows;
                break;
        }
        i      = 0;
        count2 = arg0->rowCount;
        if (count2 != 0) {
            stacks = Gp_StackLimits;
            idx    = arg1 - 0xA0;
            stack  = gpStackLimitAt(stacks, idx);
            walker = gpItemRowAt(table2, start2);
            do {
                if (walker->itemId == arg1) {
                    if (walker->qty < stack->maxHeld) {
                        found = 1;
                    } else {
                        found = 2;
                    }
                    break;
                }
                i++;
                walker++;
            } while (i < count2);
        }

        if (found == 0) {
            used++;
        }
        if (found == 2) {
            return 0;
        }
    } else {
        used++;
    }
    return used <= capacity;
}

InventoryItemRow* Gp_SetScanItem(InventoryItemRange* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    InventoryItemRow* table;
    InventoryItemRow* dest;
    s32               row;
    s32               i;
    s32               item;
    s32               qty;

    table = _inventoryGetRangeTable(arg0);
    if ((u32)(arg2 - 0xA0) < 0x20U) {
        dest = Gp_GiveItem(arg0, arg2, arg3);
        i    = arg0->firstRow;
        if (table[i + arg1].itemId == INVENTORY_ITEM_NONE) {
            row = i;
            for (i = 0; i < arg0->rowCount; i++, row++) {
                if (table[row].itemId == arg2) {
                    if (i == arg1) {
                        break;
                    }
                    dest                             = &table[arg0->firstRow + arg1];
                    dest->itemId                     = arg2;
                    table[arg0->firstRow + arg1].qty = table[row].qty;
                    table[row].itemId                = INVENTORY_ITEM_NONE;
                    table[row].qty                   = 0;
                    break;
                }
            }
        }
    } else {
        row = arg0->firstRow + arg1;
        if (table[row].itemId == INVENTORY_ITEM_NONE) {
            dest         = &table[row];
            dest->itemId = arg2;
            dest->qty    = 1;
        } else {
            dest         = &table[row];
            item         = dest->itemId;
            qty          = dest->qty;
            dest->itemId = arg2;
            dest->qty    = 1;
            Gp_GiveItem(arg0, item, qty);
        }
    }
    return dest;
}

InventoryItemRow* inventoryAddItem(const InventoryItemRange* range, s32 itemId, s32 quantity)
{
    InventoryItemRow* table;
    InventoryItemRow* addedRow;
    s32               foundStack;
    s32               rowIndex;
    s32               rangeIndex;

    table    = _inventoryGetRangeTable(range);
    addedRow = NULL;
    if (quantity < 0) {
        if ((u32)(itemId - INVENTORY_CONSUMABLE_ITEM_FIRST) < INVENTORY_CONSUMABLE_ITEM_COUNT) {
            if (quantity == INVENTORY_ADD_FULL_STACK) {
                quantity = Gp_StackLimits[itemId - INVENTORY_CONSUMABLE_ITEM_FIRST].maxHeld;
            } else {
                quantity = Gp_StackLimits[itemId - INVENTORY_CONSUMABLE_ITEM_FIRST].packQty;
            }
        } else {
            quantity = 1;
        }
    }

    // Consumables share one stack within the requested range.
    rowIndex   = range->firstRow;
    foundStack = 0;
    if ((u32)(itemId - INVENTORY_CONSUMABLE_ITEM_FIRST) < INVENTORY_CONSUMABLE_ITEM_COUNT) {
        for (rangeIndex = 0; rangeIndex < range->rowCount; rangeIndex++, rowIndex++) {
            if (table[rowIndex].itemId == itemId) {
                quantity += table[rowIndex].qty;
                if (Gp_StackLimits[itemId - INVENTORY_CONSUMABLE_ITEM_FIRST].maxHeld < quantity) {
                    quantity = Gp_StackLimits[itemId - INVENTORY_CONSUMABLE_ITEM_FIRST].maxHeld;
                }
                table[rowIndex].qty = quantity;
                addedRow            = &table[rowIndex];
                foundStack          = 1;
                break;
            }
        }
        if (foundStack) {
            return addedRow;
        }
        rowIndex = range->firstRow;
        for (rangeIndex = 0; rangeIndex < range->rowCount; rangeIndex++, rowIndex++) {
            if (table[rowIndex].itemId == INVENTORY_ITEM_NONE) {
                table[rowIndex].itemId = itemId;
                if (Gp_StackLimits[itemId - INVENTORY_CONSUMABLE_ITEM_FIRST].maxHeld < quantity) {
                    quantity = Gp_StackLimits[itemId - INVENTORY_CONSUMABLE_ITEM_FIRST].maxHeld;
                }
                addedRow             = &table[rowIndex];
                addedRow->qty        = quantity;
                addedRow->attachSlot = INVENTORY_ATTACHMENT_NONE;
                break;
            }
        }
    } else {
        for (rangeIndex = 0; rangeIndex < range->rowCount; rangeIndex++, rowIndex++) {
            if (table[rowIndex].itemId == INVENTORY_ITEM_NONE) {
                addedRow             = &table[rowIndex];
                addedRow->itemId     = itemId;
                addedRow->qty        = 1;
                addedRow->attachSlot = INVENTORY_ATTACHMENT_NONE;
                break;
            }
        }
    }
    return addedRow;
}

/// Returns whether an item uses its identified name and description.
///
/// `itemId` is a nonnegative catalogue id. Ids below 0x180 use the live save's
/// persistent identification flag; larger ids always return true. Items without
/// unidentified text start identified, and examination or use can identify others.
static inline bool _itemIsIdentified(s32 itemId)
{
    enum {
        ITEM_IDENTIFICATION_ID_LIMIT      = 0x180,
        ITEM_IDENTIFICATION_BITS_PER_WORD = 32
    };
    const McSaveData* save;
    s32               wordIndex;
    u32               bitMask;

    wordIndex = itemId / ITEM_IDENTIFICATION_BITS_PER_WORD;
    bitMask   = 1U << (itemId % ITEM_IDENTIFICATION_BITS_PER_WORD);
    if ((u32)itemId >= ITEM_IDENTIFICATION_ID_LIMIT) {
        return true;
    }
    save = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    return (save->state.itemSeenBits[wordIndex] & bitMask) != 0;
}

const u8* itemGetText(s32 itemId, s32 fieldIndex, s32 forceIdentified)
{
    enum {
        ITEM_TEXT_PACKED_ELEMENT_MASK     = 0xF0,
        ITEM_TEXT_PACKED_ENERGY_MASK      = 0x0C,
        ITEM_TEXT_PACKED_LEVEL_MASK       = 0x03,
        ITEM_TEXT_PE_ENERGIES_PER_ELEMENT = 3,
        ITEM_TEXT_PE_LEVELS_PER_ENERGY    = 3,
        ITEM_TEXT_PE_ID_FIRST             = 0x0F
    };

    const s8*       text;
    const ItemDesc* descriptor;
    s32             byte;
    s32             level;
    s32             element;
    s32             energyIndex;
    s32             groupBase;
    s32             levelOffset;

    if (itemId >= ITEM_TEXT_ENEMY_ID_FIRST) {
        text = (const s8*)Gp_ItemTextHi[itemId - ITEM_TEXT_ENEMY_ID_FIRST];
    } else if (itemId >= ITEM_TEXT_PACKED_ID_FIRST) {
        // Packed PE ids choose an element, an energy within it and a one-based level.
        // Level zero aliases level one; the recursive lookup uses identified text.
        level       = itemId & ITEM_TEXT_PACKED_LEVEL_MASK;
        element     = (itemId & ITEM_TEXT_PACKED_ELEMENT_MASK) >> 4;
        energyIndex = (itemId & ITEM_TEXT_PACKED_ENERGY_MASK) >> 2;
        if (level == 0) {
            level = 1;
        }
        groupBase   = (element * ITEM_TEXT_PE_ENERGIES_PER_ELEMENT + energyIndex) * ITEM_TEXT_PE_LEVELS_PER_ENERGY;
        levelOffset = level + (ITEM_TEXT_PE_ID_FIRST - 1);
        text        = (const s8*)itemGetText(groupBase + levelOffset, fieldIndex, 1);
    } else {
        if (itemId < ITEM_TEXT_KEY_ID_FIRST) {
            descriptor = &Gp_ItemDescs[itemId];
        } else {
            descriptor = &Gp_KeyItemDescs[(itemId)-ITEM_TEXT_KEY_ID_FIRST];
        }
        if (forceIdentified == 0) {
            forceIdentified = _itemIsIdentified(itemId);
        }
        // The parser reads signed bytes, even though catalogue text uses u8 storage.
        text = (const s8*)descriptor->textFields;
        if (fieldIndex >= ITEM_TEXT_FIELDS_PER_FORM) {
            fieldIndex = ITEM_TEXT_NAME;
        }
        if (forceIdentified == 0) {
            fieldIndex += ITEM_TEXT_FIELDS_PER_FORM;
        }
        // Skip fields, each ended by a NUL, a newline or a `\n` / `\N`
        // escape.
        for (; fieldIndex > 0; text++) {
            byte = *text;
            if (byte == '\0' || byte == '\n' || (byte == 'n' && text[-1] == '\\') || (byte == 'N' && text[-1] == '\\')) {
                fieldIndex--;
            }
        }
    }
    return (const u8*)text;
}

s32 inventoryGetNthWeaponForConsumable(const InventoryItemRange* range, s32 matchIndex, s32 consumableItemId)
{
    const InventoryItemRow* table;
    s32                     rowIndex;
    s32                     choiceIndex;
    const PlayerStatus*     playerStatus;

    table        = _inventoryGetRangeTable(range);
    rowIndex     = range->firstRow;
    playerStatus = &gPlayerStatus;
    // Count each load separately, just as the weapon-list counter does.
    while (matchIndex >= 0) {
        if ((u8)(table[rowIndex].itemId - EQUIPMENT_WEAPON_ITEM_FIRST) < ARRAY_SIZE(Gp_RelatedQty0.rows)) {
            if (consumableItemId == INVENTORY_ITEM_NONE) {
                matchIndex--;
            } else {
                for (choiceIndex = 0; choiceIndex < ARRAY_SIZE(Gp_RelatedQty0.rows[0].acceptedItemIds); choiceIndex++) {
                    if (Gp_RelatedQty0.rows[table[rowIndex].itemId - EQUIPMENT_WEAPON_ITEM_FIRST].acceptedItemIds[choiceIndex] == consumableItemId) {
                        if (table[rowIndex].attachSlot > INVENTORY_ATTACHMENT_NONE || playerStatus->weapon == table[rowIndex].itemId - (EQUIPMENT_WEAPON_ITEM_FIRST - 1)) {
                            matchIndex--;
                        }
                        break;
                    }
                }
                for (choiceIndex = 0; choiceIndex < ARRAY_SIZE(Gp_RelatedQty1.rows[0].acceptedItemIds); choiceIndex++) {
                    if (Gp_RelatedQty1.rows[table[rowIndex].itemId - EQUIPMENT_WEAPON_ITEM_FIRST].acceptedItemIds[choiceIndex] == consumableItemId) {
                        if (table[rowIndex].attachSlot > INVENTORY_ATTACHMENT_NONE || playerStatus->weapon == table[rowIndex].itemId - (EQUIPMENT_WEAPON_ITEM_FIRST - 1)) {
                            matchIndex--;
                        }
                        break;
                    }
                }
            }
        }
        rowIndex++;
    }
    rowIndex--;
    return table[rowIndex].itemId;
}

/// Clears a weapon's removable consumable selections and quantities.
///
/// Non-weapon ids do nothing. A built-in supply stays loaded, and a missing
/// secondary load keeps its unavailable marker while its quantity is cleared.
static inline void _equipmentClearRemovableLoads(s32 weaponItemId)
{
    EquipmentWeaponLoad* loads;
    s32                  hasSupply = 0;
    s32                  supplyIndex;

    if ((u32)(weaponItemId - EQUIPMENT_WEAPON_ITEM_FIRST) >= ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems)) {
        return;
    }

    loads = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems[weaponItemId - EQUIPMENT_WEAPON_ITEM_FIRST];
    for (supplyIndex = 0; supplyIndex < EQUIPMENT_WEAPON_SUPPLY_COUNT; supplyIndex++) {
        if (weaponItemId == Gp_ItemMaps[supplyIndex].weaponItemId) {
            hasSupply = 1;
            break;
        }
    }

    if ((hasSupply == 0) || (Gp_ItemMaps[supplyIndex].supplyLoad != EQUIPMENT_WEAPON_SUPPLY_PRIMARY)) {
        loads->primaryItemId = INVENTORY_ITEM_NONE;
        loads->primaryQty    = 0;
    }

    if ((hasSupply == 0) || (Gp_ItemMaps[supplyIndex].supplyLoad != EQUIPMENT_WEAPON_SUPPLY_SECONDARY)) {
        if (loads->secondaryItemId != EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE) {
            loads->secondaryItemId = INVENTORY_ITEM_NONE;
        }
        loads->secondaryQty = 0;
    }
}

void inventoryDetachItem(InventoryItemRow* row)
{
    u8  weaponItemId;
    s32 isWeapon;

    if (row->attachSlot <= INVENTORY_ATTACHMENT_NONE) {
        return;
    }

    // Clear the attachment before reloading the item id for weapon-load cleanup.
    isWeapon        = (u8)(row->itemId + EQUIPMENT_WEAPON_ITEM_FIRST) < ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems);
    row->attachSlot = INVENTORY_ATTACHMENT_NONE;
    if (!isWeapon) {
        return;
    }

    weaponItemId = row->itemId;
    if (weaponItemId == gPlayerStatus.weapon + (EQUIPMENT_WEAPON_ITEM_FIRST - 1)) {
        return;
    }

    _equipmentClearRemovableLoads(weaponItemId);
}

void func_800B92CC(Task* task)
{
    switch (GAME_LOCATION_WORD(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc) & GAME_LOCATION_STAGE_AREA_MASK) {
        case GAME_LOCATION_KEY(1, 1, 0, 0):
            func_acropolis_square_80180804(task);
            break;
        case GAME_LOCATION_KEY(1, 15, 0, 0):
            func_acropolis_fire_escape_8017EA68(task);
            break;
        case GAME_LOCATION_KEY(1, 19, 0, 0):
            func_mist_parking_80181468(task);
            break;
        case GAME_LOCATION_KEY(2, 1, 0, 0):
            func_dryfield_gas_station_8017EA90(task);
            break;
        case GAME_LOCATION_KEY(2, 17, 0, 0):
            func_dryfield_motel_lobby_8017E9E8(task);
            break;
        case GAME_LOCATION_KEY(2, 27, 0, 0):
            func_dryfield_trailer_coach_80181364(task);
            break;
        case GAME_LOCATION_KEY(3, 1, 0, 0):
            func_dryfield_night_gas_station_8017E9F8(task);
            break;
        case GAME_LOCATION_KEY(3, 17, 0, 0):
            func_dryfield_night_motel_lobby_8017EAE0(task);
            break;
        case GAME_LOCATION_KEY(3, 27, 0, 0):
            func_dryfield_night_trailer_coach_8018138C(task);
            break;
        case GAME_LOCATION_KEY(4, 6, 0, 0):
            func_mine_refuge_8017EA78(task);
            break;
        case GAME_LOCATION_KEY(4, 16, 0, 0):
            func_shelter_b1_sterilization_room_8017EB2C(task);
            break;
        case GAME_LOCATION_KEY(4, 20, 0, 0):
            func_shelter_b1_underground_parking_8017EDE8(task);
            break;
        case GAME_LOCATION_KEY(4, 31, 0, 0):
            func_shelter_b2_laboratory_8017EAB4(task);
            break;
        case GAME_LOCATION_KEY(4, 41, 0, 0):
            func_shelter_b3_incinerator_control_room_8017EA64(task);
            break;
        case GAME_LOCATION_KEY(4, 47, 0, 0):
            func_shelter_r47_8017EC04(task);
            break;
        case GAME_LOCATION_KEY(5, 22, 0, 0):
            func_shelter_b6_nursery_8017EAC4(task);
            break;
        case GAME_LOCATION_KEY(5, 28, 0, 0):
            func_shelter_1f_tent_8017EA60(task);
            break;
        case GAME_LOCATION_KEY(2, 30, 0, 0):
            func_dryfield_motel_room_6_8017EA58(task);
            break;
        case GAME_LOCATION_KEY(3, 30, 0, 0):
            func_dryfield_night_motel_room_6_8017EA74(task);
            break;
    }
}

/// "Notice". The byte after the terminator is not zero: the original toolchain
/// left it in the alignment gap.
