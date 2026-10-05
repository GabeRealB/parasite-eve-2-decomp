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

/// The element an accessor was handed. An inlined function's argument is
/// expanded as an address, scaled index first, which is the order the
/// callers' element addresses have (see `gpAreaPlaceRef`).
static inline InventoryConsumableStack* gpStackLimitRef(InventoryConsumableStack* row)
{
    return row;
}

/// Row `index` elements after `rows`.
/// The result borrows `rows`; `index` must name a row in that table.
#define gpStackLimitAt(rows, index) gpStackLimitRef(&(rows)[index])

/* Item table a scan window lies in. */
static inline InventoryItemRow* _gpScanTable(InventoryItemRange* scan);

/// True if `arg2` of item `arg1` can be added to the item table selected
/// by `arg0`. Ids `>= 0x100` always succeed. Ids `0xA0..0xFF` stack onto
/// an existing row when `qty + arg2` fits `Gp_StackLimits[id-0xA0].maxHeld`;
/// `arg2 < 0` uses that row's `packQty` as the addend. Other ids need a
/// free slot.
static s32 Gp_CanAddItemQty(InventoryItemRange* arg0, s32 arg1, s32 arg2);

static inline bool _itemIsIdentified(s32 itemId);

static inline void _gpClearEquipSlot(s32 item);

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

/* Item table a scan window lies in. */
static inline InventoryItemRow* _gpScanTable(InventoryItemRange* scan)
{
    InventoryItemRow* table;

    switch (scan->tableId) {
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

void Gp_SortItems(InventoryItemRange* arg0, s32 arg1)
{
    InventoryItemRow*          tmp;
    register InventoryItemRow* table;
    InventoryItemRow*          rec;
    InventoryItemRow*          other;
    InventoryItemRow           saved;
    s32                        i;
    s32                        j;
    s32                        key;
    s32                        minKey;
    s32                        id;
    s32                        idx;
    s32                        count;
    s32                        dummy5;
    s32                        dummy6;
    s32                        dummy7;

    i = 0;
    if ((arg0->rowCount - 1) > 0) {
        do {
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
            table = tmp;
            rec   = table + arg0->firstRow;
            rec  += i;
            id    = rec->itemId;

            key = 0;
            if (id == 0) {
                key = 0x1000;
            } else if ((u32)(id - 1) < 0x5F) {
                key = Gp_ItemSortKey0[id];
            } else {
                idx = id - 0x60;
                if ((u32)idx < 0x20) {
                    key = Gp_ItemSortKey60[idx];
                } else {
                    idx = id - 0x80;
                    if ((u32)idx < 0x20) {
                        key = Gp_ItemSortKey80[idx];
                    } else {
                        idx = id - 0xA0;
                        if ((u32)idx < 0x20) {
                            key = Gp_ItemSortKeyA0[idx];
                        }
                    }
                }
            }
            if (key == 0) {
                key = id + 0x100;
            }
            minKey = key;

            if (arg0->tableId != INVENTORY_ITEM_TABLE_INDIRECT) {
                tmp = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemRows;
                if (arg0->tableId == INVENTORY_ITEM_TABLE_AREA_GRANTS) {
                    tmp = Gp_ItemTable2;
                }
            } else {
                tmp = Gp_ItemTable1;
            }
            table = tmp;
            j     = i + 1;
            // Start just after the row being placed.
            other  = table + arg0->firstRow;
            other += i + 1;
            if (j < arg0->rowCount) {
                do {
                    id = other->itemId;

                    key = 0;
                    if (id == 0) {
                        key = 0x1000;
                    } else if ((u32)(id - 1) < 0x5F) {
                        key = Gp_ItemSortKey0[id];
                    } else {
                        idx = id - 0x60;
                        if ((u32)idx < 0x20) {
                            key = Gp_ItemSortKey60[idx];
                        } else {
                            idx = id - 0x80;
                            if ((u32)idx < 0x20) {
                                key = Gp_ItemSortKey80[idx];
                            } else {
                                idx = id - 0xA0;
                                if ((u32)idx < 0x20) {
                                    key = Gp_ItemSortKeyA0[idx];
                                }
                            }
                        }
                    }
                    if (key == 0) {
                        key = id + 0x100;
                    }
                    if (key < minKey) {
                        minKey = key;
                        saved  = *rec;
                        *rec   = *other;
                        *other = saved;
                    }
                    count = arg0->rowCount;
                    j++;
                    other++;
                } while (j < count);
            }
            count = arg0->rowCount;
            i++;
        } while (i < (count - 1));
    }
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

    table = _gpScanTable(arg0);
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

/// Adds `arg2` of item `arg1` to the item table selected by `arg0`.
/// Ids `0xA0..0xBF` stack onto an existing row, clamped to
/// `Gp_StackLimits[id-0xA0].maxHeld`. `arg2 < 0` uses that row's `packQty`
/// as the count, or `maxHeld` when `arg2 == -2`; out-of-range ids use 1.
/// Other ids take the first free slot with quantity 1. Returns the
/// written row, or NULL if none was free.
InventoryItemRow* Gp_AddItem(InventoryItemRange* arg0, s32 arg1, s32 arg2)
{
    InventoryItemRow* table;
    InventoryItemRow* dest;
    s32               found;
    s32               row;
    s32               i;

    table = _gpScanTable(arg0);
    dest  = NULL;
    if (arg2 < 0) {
        if ((u32)(arg1 - 0xA0) < 0x20) {
            if (arg2 == -2) {
                arg2 = Gp_StackLimits[arg1 - 0xA0].maxHeld;
            } else {
                arg2 = Gp_StackLimits[arg1 - 0xA0].packQty;
            }
        } else {
            arg2 = 1;
        }
    }

    row   = arg0->firstRow;
    found = 0;
    if ((u32)(arg1 - 0xA0) < 0x20) {
        for (i = 0; i < arg0->rowCount; i++, row++) {
            if (table[row].itemId == arg1) {
                arg2 += table[row].qty;
                if (Gp_StackLimits[arg1 - 0xA0].maxHeld < arg2) {
                    arg2 = Gp_StackLimits[arg1 - 0xA0].maxHeld;
                }
                table[row].qty = arg2;
                dest           = &table[row];
                found          = 1;
                break;
            }
        }
        if (found) {
            return dest;
        }
        row = arg0->firstRow;
        for (i = 0; i < arg0->rowCount; i++, row++) {
            if (table[row].itemId == INVENTORY_ITEM_NONE) {
                table[row].itemId = arg1;
                if (Gp_StackLimits[arg1 - 0xA0].maxHeld < arg2) {
                    arg2 = Gp_StackLimits[arg1 - 0xA0].maxHeld;
                }
                dest             = &table[row];
                dest->qty        = arg2;
                dest->attachSlot = INVENTORY_ATTACHMENT_NONE;
                break;
            }
        }
    } else {
        for (i = 0; i < arg0->rowCount; i++, row++) {
            if (table[row].itemId == INVENTORY_ITEM_NONE) {
                dest             = &table[row];
                dest->itemId     = arg1;
                dest->qty        = 1;
                dest->attachSlot = INVENTORY_ATTACHMENT_NONE;
                break;
            }
        }
    }
    return dest;
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

char* Gp_GetItemText(s32 arg0, s32 arg1, s32 arg2)
{
    const s8*       str;
    const ItemDesc* desc;
    s32             c;
    s32             n;
    s32             row;
    s32             col;
    s32             id;
    s32             ofs;

    if (arg0 >= 0x500) {
        str = (const s8*)Gp_ItemTextHi[arg0 - 0x500];
    } else if (arg0 >= 0x300) {
        // A packed id: bits 4-7 and 2-3 pick a run of three entries starting
        // at id 0xF, bits 0-1 the entry within it (1-3, with 0 read as 1).
        n   = arg0 & 3;
        row = (arg0 & 0xF0) >> 4;
        col = (arg0 & 0xC) >> 2;
        if (n == 0) {
            n = 1;
        }
        id  = (row * 3 + col) * 3;
        ofs = n + 0xE;
        str = (const s8*)Gp_GetItemText(id + ofs, arg1, 1);
    } else {
        if (arg0 < 0x100) {
            desc = &Gp_ItemDescs[arg0];
        } else {
            desc = &Gp_KeyItemDescs[(arg0)-0x100];
        }
        if (arg2 == 0) {
            arg2 = _itemIsIdentified(arg0);
        }
        // The parser reads signed bytes, even though catalogue text uses u8 storage.
        str = (const s8*)desc->textFields;
        if (arg1 >= 3) {
            arg1 = 0;
        }
        if (arg2 == 0) {
            arg1 += 3;
        }
        // Skip `arg1` fields, each ended by a NUL, a newline or a `\n` / `\N`
        // escape.
        for (; arg1 > 0; str++) {
            c = *str;
            if (c == '\0' || c == '\n' || (c == 'n' && str[-1] == '\\') || (c == 'N' && str[-1] == '\\')) {
                arg1--;
            }
        }
    }
    return (char*)str;
}

s32 Gp_NthRelatedId(InventoryItemRange* arg0, s32 arg1, s32 arg2)
{
    InventoryItemRow* table;
    s32               idx;
    s32               i;
    PlayerStatus*     cfg;

    table = _gpScanTable(arg0);
    idx   = arg0->firstRow;
    cfg   = &gPlayerStatus;
    while (arg1 >= 0) {
        if ((u8)(table[idx].itemId - EQUIPMENT_WEAPON_ITEM_FIRST) < ARRAY_SIZE(Gp_RelatedQty0.rows)) {
            if (arg2 == 0) {
                arg1--;
            } else {
                for (i = 0; i < ARRAY_SIZE(Gp_RelatedQty0.rows[0].acceptedItemIds); i++) {
                    if (Gp_RelatedQty0.rows[table[idx].itemId - EQUIPMENT_WEAPON_ITEM_FIRST].acceptedItemIds[i] == arg2) {
                        if (table[idx].attachSlot > INVENTORY_ATTACHMENT_NONE || cfg->weapon == table[idx].itemId - 0x7F) {
                            arg1--;
                        }
                        break;
                    }
                }
                for (i = 0; i < ARRAY_SIZE(Gp_RelatedQty0.rows[0].acceptedItemIds); i++) {
                    if (Gp_RelatedQty1.rows[table[idx].itemId - EQUIPMENT_WEAPON_ITEM_FIRST].acceptedItemIds[i] == arg2) {
                        if (table[idx].attachSlot > INVENTORY_ATTACHMENT_NONE || cfg->weapon == table[idx].itemId - 0x7F) {
                            arg1--;
                        }
                        break;
                    }
                }
            }
        }
        idx++;
    }
    idx--;
    return table[idx].itemId;
}

/// Empties the removable consumable loads of weapon item `item`, the same clear
/// `Gp_ClearEquipSlot` performs. A built-in supply, when this weapon has one,
/// stays in its load.
static inline void _gpClearEquipSlot(s32 item)
{
    EquipmentWeaponLoad* slot;
    s32                  found = 0;
    s32                  i;

    if ((u32)(item - 0x80) >= 0x20) {
        return;
    }

    slot = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems[item - EQUIPMENT_WEAPON_ITEM_FIRST];
    for (i = 0; i < EQUIPMENT_WEAPON_SUPPLY_COUNT; i++) {
        if (item == Gp_ItemMaps[i].weaponItemId) {
            found = 1;
            break;
        }
    }

    if ((found == 0) || (Gp_ItemMaps[i].supplyLoad != EQUIPMENT_WEAPON_SUPPLY_PRIMARY)) {
        slot->primaryItemId = INVENTORY_ITEM_NONE;
        slot->primaryQty    = 0;
    }

    if ((found == 0) || (Gp_ItemMaps[i].supplyLoad != EQUIPMENT_WEAPON_SUPPLY_SECONDARY)) {
        if (slot->secondaryItemId != EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE) {
            slot->secondaryItemId = INVENTORY_ITEM_NONE;
        }
        slot->secondaryQty = 0;
    }
}

void Gp_RefreshItemRow(InventoryItemRow* arg0)
{
    u8  item;
    s32 inRange;

    if (arg0->attachSlot <= INVENTORY_ATTACHMENT_NONE) {
        return;
    }

    inRange          = (u8)(arg0->itemId + 0x80) < 0x20;
    arg0->attachSlot = INVENTORY_ATTACHMENT_NONE;
    if (!inRange) {
        return;
    }

    item = arg0->itemId;
    if (item == gPlayerStatus.weapon + 0x7F) {
        return;
    }

    _gpClearEquipSlot(item);
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
