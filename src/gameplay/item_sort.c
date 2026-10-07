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

/// First item id whose possession uses collection bits rather than item rows.
enum { INVENTORY_COLLECTION_ITEM_FIRST = 0x100 };

/// Result of checking the first matching consumable stack for room.
enum {
    INVENTORY_STACK_ABSENT      = 0,
    INVENTORY_STACK_ACCEPTS_ADD = 1,
    INVENTORY_STACK_REJECTS_ADD = 2
};

static s32 _inventoryCanAddItemQuantity(const InventoryItemRange* range, s32 itemId, s32 quantity);

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

/// Returns 1 if a requested addition fits an existing stack or has a free row.
///
/// Quantity counts item units; every negative value requests one catalogue pack,
/// including `INVENTORY_GIVE_FULL_STACK`. The first matching stack must fit the entire
/// addition. A new stack only needs a free row: its eventual capacity clamp is
/// not checked here. Other row items need a free row regardless of quantity.
/// Ids >= 0x100 return 1 after the occupied-row scan, without testing possession.
/// Row-item ids must be 1..0xBF; 0xC0..0xFF are not bounds-checked and cannot
/// index the 32-row consumable catalogue safely. The range must fit readable
/// backing storage. Neither the descriptor nor its rows are changed or retained.
static s32 _inventoryCanAddItemQuantity(const InventoryItemRange* range, s32 itemId, s32 quantity)
{
    const InventoryItemRow*         table;
    const InventoryItemRow*         row;
    s32                             rangeIndex;
    s32                             occupiedRows;
    s32                             rowCount;
    s32                             firstRow;
    s32                             scanRowCount;
    s32                             requiredRows;
    s32                             stackResult;
    const InventoryItemRow*         stackTable;
    const InventoryItemRow*         stackRow;
    s32                             stackRowCount;
    s32                             stackFirstRow;
    const InventoryConsumableStack* stackCatalogue;
    s32                             consumableIndex;
    const InventoryConsumableStack* stackLimits;
    s32                             rowCapacity;

    // Count occupied rows before deciding whether another row is needed.
    table        = _inventoryGetRangeTable(range);
    rangeIndex   = 0;
    rowCount     = range->rowCount;
    firstRow     = range->firstRow;
    occupiedRows = 0;
    if (rowCount != 0) {
        scanRowCount = rowCount;
        row          = gpItemRowAt(table, firstRow);
        do {
            if (row->itemId != INVENTORY_ITEM_NONE) {
                occupiedRows++;
            }
            rangeIndex++;
            row++;
        } while (rangeIndex < scanRowCount);
    }

    rowCapacity  = range->rowCount;
    requiredRows = occupiedRows;
    if (itemId >= INVENTORY_COLLECTION_ITEM_FIRST) {
        return 1;
    }

    // The first matching consumable stack decides whether the addition fits.
    if (itemId >= INVENTORY_CONSUMABLE_ITEM_FIRST) {
        stackResult   = INVENTORY_STACK_ABSENT;
        stackFirstRow = range->firstRow;
        stackTable    = _inventoryGetRangeTable(range);
        if (quantity < 0) {
            quantity = Gp_StackLimits[itemId - INVENTORY_CONSUMABLE_ITEM_FIRST].packQty;
        }
        rangeIndex    = 0;
        stackRowCount = range->rowCount;
        if (stackRowCount != 0) {
            stackCatalogue  = Gp_StackLimits;
            consumableIndex = itemId - INVENTORY_CONSUMABLE_ITEM_FIRST;
            stackLimits     = gpStackLimitAt(stackCatalogue, consumableIndex);
            stackRow        = gpItemRowAt(stackTable, stackFirstRow);
            do {
                if (stackRow->itemId == itemId) {
                    stackResult = INVENTORY_STACK_REJECTS_ADD;
                    if (stackLimits->maxHeld >= stackRow->qty + quantity) {
                        stackResult = INVENTORY_STACK_ACCEPTS_ADD;
                    }
                    break;
                }
                rangeIndex++;
                stackRow++;
            } while (rangeIndex < stackRowCount);
        }

        if (stackResult == INVENTORY_STACK_ABSENT) {
            requiredRows++;
        }
        if (stackResult == INVENTORY_STACK_REJECTS_ADD) {
            return 0;
        }
    } else {
        requiredRows++;
    }
    return requiredRows <= rowCapacity;
}

s32 inventoryCanAddItem(const InventoryItemRange* range, s32 itemId)
{
    const InventoryItemRow*         table;
    const InventoryItemRow*         row;
    s32                             rangeIndex;
    s32                             occupiedRows;
    s32                             rowCount;
    s32                             firstRow;
    s32                             scanRowCount;
    s32                             requiredRows;
    s32                             stackResult;
    const InventoryItemRow*         stackTable;
    const InventoryItemRow*         stackRow;
    s32                             stackRowCount;
    s32                             stackFirstRow;
    const InventoryConsumableStack* stackCatalogue;
    s32                             consumableIndex;
    const InventoryConsumableStack* stackLimits;
    s32                             rowCapacity;

    // Count occupied rows before deciding whether another row is needed.
    table        = _inventoryGetRangeTable(range);
    rangeIndex   = 0;
    rowCount     = range->rowCount;
    firstRow     = range->firstRow;
    occupiedRows = 0;
    if (rowCount != 0) {
        scanRowCount = rowCount;
        row          = gpItemRowAt(table, firstRow);
        do {
            if (row->itemId != INVENTORY_ITEM_NONE) {
                occupiedRows++;
            }
            rangeIndex++;
            row++;
        } while (rangeIndex < scanRowCount);
    }

    rowCapacity  = range->rowCount;
    requiredRows = occupiedRows;
    if (itemId >= INVENTORY_COLLECTION_ITEM_FIRST) {
        return 1;
    }

    // The first matching consumable stack decides whether the addition fits.
    if (itemId >= INVENTORY_CONSUMABLE_ITEM_FIRST) {
        stackResult   = INVENTORY_STACK_ABSENT;
        stackFirstRow = range->firstRow;
        stackTable    = _inventoryGetRangeTable(range);
        rangeIndex    = 0;
        stackRowCount = range->rowCount;
        if (stackRowCount != 0) {
            stackCatalogue  = Gp_StackLimits;
            consumableIndex = itemId - INVENTORY_CONSUMABLE_ITEM_FIRST;
            stackLimits     = gpStackLimitAt(stackCatalogue, consumableIndex);
            stackRow        = gpItemRowAt(stackTable, stackFirstRow);
            do {
                if (stackRow->itemId == itemId) {
                    if (stackRow->qty < stackLimits->maxHeld) {
                        stackResult = INVENTORY_STACK_ACCEPTS_ADD;
                    } else {
                        stackResult = INVENTORY_STACK_REJECTS_ADD;
                    }
                    break;
                }
                rangeIndex++;
                stackRow++;
            } while (rangeIndex < stackRowCount);
        }

        if (stackResult == INVENTORY_STACK_ABSENT) {
            requiredRows++;
        }
        if (stackResult == INVENTORY_STACK_REJECTS_ADD) {
            return 0;
        }
    } else {
        requiredRows++;
    }
    return requiredRows <= rowCapacity;
}

InventoryItemRow* inventoryPlaceItemAtRow(const InventoryItemRange* range, s32 rowIndex, s32 itemId, s32 quantity)
{
    InventoryItemRow* table;
    InventoryItemRow* placedRow;
    s32               tableIndex;
    s32               rangeIndex;
    s32               displacedItemId;
    s32               displacedQuantity;

    /// Moves a stack's id and quantity to an empty row, retaining both attachments.
    ///
    /// `destinationIndex` is relative to `itemRange`; `sourceIndex` is absolute
    /// in its writable `rows` table. The rows must be distinct and in bounds.
    /// Arguments must be side-effect-free scalars; `result` is a separate row
    /// pointer lvalue. Rows, range and result are evaluated repeatedly. Use as
    /// a statement inside braces. This binding is limited to this function.
#define INVENTORY_RELOCATE_STACK(rows, itemRange, destinationIndex, sourceIndex, incomingId, result)                  \
    {                                                                                                                 \
        (result)                                               = &(rows)[(itemRange)->firstRow + (destinationIndex)]; \
        (result)->itemId                                       = (incomingId);                                        \
        (rows)[(itemRange)->firstRow + (destinationIndex)].qty = (rows)[(sourceIndex)].qty;                           \
        (rows)[(sourceIndex)].itemId                           = INVENTORY_ITEM_NONE;                                 \
        (rows)[(sourceIndex)].qty                              = 0;                                                   \
    }

    table = _inventoryGetRangeTable(range);
    if ((u32)(itemId - INVENTORY_CONSUMABLE_ITEM_FIRST) < (u32)INVENTORY_CONSUMABLE_ITEM_COUNT) {
        // Merge first; an empty requested row can then receive the whole stack.
        placedRow  = inventoryGiveItem(range, itemId, quantity);
        rangeIndex = range->firstRow;
        if (table[rangeIndex + rowIndex].itemId == INVENTORY_ITEM_NONE) {
            tableIndex = rangeIndex;
            for (rangeIndex = 0; rangeIndex < range->rowCount; rangeIndex++, tableIndex++) {
                if (table[tableIndex].itemId == itemId) {
                    if (rangeIndex == rowIndex) {
                        break;
                    }
                    INVENTORY_RELOCATE_STACK(table, range, rowIndex, tableIndex, itemId, placedRow);
                    break;
                }
            }
        }
    } else {
        // Replace this row and try to preserve the displaced item elsewhere.
        tableIndex = range->firstRow + rowIndex;
        if (table[tableIndex].itemId == INVENTORY_ITEM_NONE) {
            placedRow         = &table[tableIndex];
            placedRow->itemId = itemId;
            placedRow->qty    = 1;
        } else {
            placedRow         = &table[tableIndex];
            displacedItemId   = placedRow->itemId;
            displacedQuantity = placedRow->qty;
            placedRow->itemId = itemId;
            placedRow->qty    = 1;
            inventoryGiveItem(range, displacedItemId, displacedQuantity);
        }
    }
    return placedRow;
#undef INVENTORY_RELOCATE_STACK
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
            shelterR47TelephoneMenuTask(task);
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
