#include "gameplay/items.h"

#include "types.h"

#include "attachments.h"
#include "gameplay/inventory.h"
#include "inventory.h"
#include "items.h"

#include "debug/debug.h"

#include "main/mc.h"
#include "main/session.h"
#include "main/task.h"
#include "main/wipsys.h"

/// Resolve a row in the PS1 stack-limit table address space. Address words preserve
/// the runtime table base; row fields are always accessed through GpItemA0.
static inline GpItemA0* gpStackLimitAt(GpItemA0* rows, s32 index)
{
    union {
        GpItemA0* rows;
        u32       word;
    } base;
    union {
        GpItemA0* row;
        u32       word;
    } result;
    base.rows    = rows;
    result.word  = index * sizeof(GpItemA0);
    result.word += base.word;
    return result.row;
}

/* Item table a scan window lies in. */
static inline McItemRec* _gpScanTable(McItemScan* scan);

/// True if `arg2` of item `arg1` can be added to the item table selected
/// by `arg0`. Ids `>= 0x100` always succeed. Ids `0xA0..0xFF` stack onto
/// an existing row when `qty + arg2` fits `Gp_StackLimits[id-0xA0].maxHeld`;
/// `arg2 < 0` uses that row's `field_0` as the addend. Other ids need a
/// free slot.
static s32 Gp_CanAddItemQty(McItemScan* arg0, s32 arg1, s32 arg2);

/// Whether item `item` has been seen. Ids at or above 0x180 have no bit in
/// the save and always count as seen. Inline form of `Gp_HasItemSeenBit`.
static inline s32 _gpHasItemSeenBit(s32 item);

/// Empties the ammunition and attachment of weapon item `item`, the same clear
/// `Gp_ClearEquipSlot` performs.
static inline void _gpClearEquipSlot(s32 item);

void func_80180804(void);

void func_8017EA68(void);

void func_80181468(void);

void func_8017EA90(void);

void func_8017E9E8(void);

void func_80181364(void);

void func_8017EA58(void);

void func_8017E9F8(void);

void func_8017EAE0(void);

void func_8018138C(void);

void func_8017EA74(void);

void func_8017EA78(void);

void func_8017EB2C(void);

void func_8017EDE8(void);

void func_8017EAB4(void);

void func_8017EA64(void);

void func_8017EC04(void);

void func_8017EAC4(void);

void func_8017EA60(void);

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

/* Total quantity of item `id` held, via a fresh scan covering every row. */
#define GP_TOTAL_QTY(scan, id) (memset(&(scan), 0, sizeof(scan)), (scan).rowCount = 0xFF, Gp_SumScanQty(&(scan), (id)))

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
static inline McItemRec* _gpScanTable(McItemScan* scan)
{
    McItemRec* table;

    switch (scan->table) {
        case 2:
            table = Gp_ItemTable2;
            break;
        case 1:
            table = Gp_ItemTable1;
            break;
        default:
            table = Mc_SaveData[0].itemRows;
            break;
    }
    return table;
}

void Gp_SortItems(McItemScan* arg0, s32 arg1)
{
    McItemRec*          tmp;
    register McItemRec* table;
    McItemRec*          rec;
    McItemRec*          other;
    McItemRec           saved;
    s32                 i;
    s32                 j;
    s32                 key;
    s32                 minKey;
    s32                 id;
    s32                 idx;
    s32                 next;
    s32                 count;
    s32                 dummy5;
    s32                 dummy6;
    s32                 dummy7;

    GpItemRowAddress cursor;
    i = 0;
    if ((arg0->rowCount - 1) > 0) {
        do {
            switch (arg0->table) {
                case 2:
                    tmp = Gp_ItemTable2;
                    break;
                case 1:
                    tmp = Gp_ItemTable1;
                    break;
                default:
                    tmp = Mc_SaveData[0].itemRows;
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

            if (arg0->table != 1) {
                tmp = Mc_SaveData[0].itemRows;
                if (arg0->table == 2) {
                    tmp = Gp_ItemTable2;
                }
            } else {
                tmp = Gp_ItemTable1;
            }
            table        = tmp;
            j            = i + 1;
            other        = table + arg0->firstRow;
            next         = i * (s32)sizeof(*other) + (s32)sizeof(*other);
            cursor.row   = other;
            cursor.word += next;
            other        = cursor.row;
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
/// `arg2 < 0` uses that row's `field_0` as the addend. Other ids need a
/// free slot.
static s32 Gp_CanAddItemQty(McItemScan* arg0, s32 arg1, s32 arg2)
{
    McItemRec* tmp;
    McItemRec* table;
    McItemRec* rec;
    s32        i;
    s32        occupied;
    s32        count;
    s32        start;
    s32        limit;
    s32        used;
    s32        found;
    McItemRec* table2;
    McItemRec* walker;
    s32        count2;
    s32        start2;
    GpItemA0*  p;
    s32        idx;
    GpItemA0*  cap;
    s32        capacity;

    switch (arg0->table) {
        case 2:
            tmp = Gp_ItemTable2;
            break;
        case 1:
            tmp = Gp_ItemTable1;
            break;
        default:
            tmp = Mc_SaveData[0].itemRows;
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
            if (rec->itemId != 0) {
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
        switch (arg0->table) {
            case 2:
                table2 = Gp_ItemTable2;
                break;
            case 1:
                table2 = Gp_ItemTable1;
                break;
            default:
                table2 = Mc_SaveData[0].itemRows;
                break;
        }
        if (arg2 < 0) {
            arg2 = Gp_StackLimits[arg1 - 0xA0].perBuy;
        }
        i      = 0;
        count2 = arg0->rowCount;
        if (count2 != 0) {
            p      = Gp_StackLimits;
            idx    = arg1 - 0xA0;
            cap    = gpStackLimitAt(p, idx);
            walker = gpItemRowAt(table2, start2);
            do {
                if (walker->itemId == arg1) {
                    found = 2;
                    if (cap->maxHeld >= walker->qty + arg2) {
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

s32 Gp_CanAddItem(McItemScan* arg0, s32 arg1)
{
    McItemRec* tmp;
    McItemRec* table;
    McItemRec* rec;
    s32        i;
    s32        occupied;
    s32        count;
    s32        start;
    s32        limit;
    s32        used;
    s32        found;
    McItemRec* table2;
    McItemRec* walker;
    s32        count2;
    s32        start2;
    GpItemA0*  p;
    s32        idx;
    GpItemA0*  cap;
    s32        capacity;

    switch (arg0->table) {
        case 2:
            tmp = Gp_ItemTable2;
            break;
        case 1:
            tmp = Gp_ItemTable1;
            break;
        default:
            tmp = Mc_SaveData[0].itemRows;
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
            if (rec->itemId != 0) {
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
        switch (arg0->table) {
            case 2:
                table2 = Gp_ItemTable2;
                break;
            case 1:
                table2 = Gp_ItemTable1;
                break;
            default:
                table2 = Mc_SaveData[0].itemRows;
                break;
        }
        i      = 0;
        count2 = arg0->rowCount;
        if (count2 != 0) {
            p      = Gp_StackLimits;
            idx    = arg1 - 0xA0;
            cap    = gpStackLimitAt(p, idx);
            walker = gpItemRowAt(table2, start2);
            do {
                if (walker->itemId == arg1) {
                    if (walker->qty < cap->maxHeld) {
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

McItemRec* Gp_SetScanItem(McItemScan* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    McItemRec* table;
    McItemRec* dest;
    s32        row;
    s32        i;
    s32        item;
    s32        qty;

    table = _gpScanTable(arg0);
    if ((u32)(arg2 - 0xA0) < 0x20U) {
        dest = Gp_GiveItem(arg0, arg2, arg3);
        i    = arg0->firstRow;
        if (table[i + arg1].itemId == 0) {
            row = i;
            for (i = 0; i < arg0->rowCount; i++, row++) {
                if (table[row].itemId == arg2) {
                    if (i == arg1) {
                        break;
                    }
                    dest                             = &table[arg0->firstRow + arg1];
                    dest->itemId                     = arg2;
                    table[arg0->firstRow + arg1].qty = table[row].qty;
                    table[row].itemId                = 0;
                    table[row].qty                   = 0;
                    break;
                }
            }
        }
    } else {
        row = arg0->firstRow + arg1;
        if (table[row].itemId == 0) {
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
/// `Gp_StackLimits[id-0xA0].maxHeld`. `arg2 < 0` uses that row's `field_0`
/// as the count, or `field_2` when `arg2 == -2`; out-of-range ids use 1.
/// Other ids take the first free slot with quantity 1. Returns the
/// written row, or NULL if none was free.
McItemRec* Gp_AddItem(McItemScan* arg0, s32 arg1, s32 arg2)
{
    McItemRec* table;
    McItemRec* dest;
    s32        found;
    s32        row;
    s32        i;

    table = _gpScanTable(arg0);
    dest  = NULL;
    if (arg2 < 0) {
        if ((u32)(arg1 - 0xA0) < 0x20) {
            if (arg2 == -2) {
                arg2 = Gp_StackLimits[arg1 - 0xA0].maxHeld;
            } else {
                arg2 = Gp_StackLimits[arg1 - 0xA0].perBuy;
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
            if (table[row].itemId == 0) {
                table[row].itemId = arg1;
                if (Gp_StackLimits[arg1 - 0xA0].maxHeld < arg2) {
                    arg2 = Gp_StackLimits[arg1 - 0xA0].maxHeld;
                }
                dest             = &table[row];
                dest->qty        = arg2;
                dest->attachSlot = 0;
                break;
            }
        }
    } else {
        for (i = 0; i < arg0->rowCount; i++, row++) {
            if (table[row].itemId == 0) {
                dest             = &table[row];
                dest->itemId     = arg1;
                dest->qty        = 1;
                dest->attachSlot = 0;
                break;
            }
        }
    }
    return dest;
}

/// Whether item `item` has been seen. Ids at or above 0x180 have no bit in
/// the save and always count as seen. Inline form of `Gp_HasItemSeenBit`.
static inline s32 _gpHasItemSeenBit(s32 item)
{
    McSaveData* p;
    s32         word;
    s32         bit;
    s32         val;

    word = item / 32;
    bit  = 1 << (item % 32);
    if ((u32)item >= 0x180) {
        return 1;
    }
    p   = &Mc_SaveData[0];
    val = p->itemSeenBits[word] & bit;
    return val != 0;
}

char* Gp_GetItemText(s32 arg0, s32 arg1, s32 arg2)
{
    s8*         str;
    GpItemDesc* desc;
    s32         c;
    s32         n;
    s32         row;
    s32         col;
    s32         id;
    s32         ofs;

    if (arg0 >= 0x500) {
        str = Gp_ItemTextHi[arg0 - 0x500];
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
        str = Gp_GetItemText(id + ofs, arg1, 1);
    } else {
        if (arg0 < 0x100) {
            desc = &Gp_ItemDescs[arg0];
        } else {
            desc = &Gp_KeyItemDescs[(arg0)-0x100];
        }
        if (arg2 == 0) {
            arg2 = _gpHasItemSeenBit(arg0);
        }
        str = desc->field_4;
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
    return str;
}

s32 Gp_NthRelatedId(McItemScan* arg0, s32 arg1, s32 arg2)
{
    McItemRec*    table;
    s32           idx;
    s32           i;
    PlayerStatus* cfg;

    table = _gpScanTable(arg0);
    idx   = arg0->firstRow;
    cfg   = &Player_Status;
    while (arg1 >= 0) {
        if ((u8)(table[idx].itemId + 0x80) < 0x20) {
            if (arg2 == 0) {
                arg1--;
            } else {
                for (i = 0; i < 3; i++) {
                    if (Gp_RelatedQty0.rows[table[idx].itemId - 0x80].related[i] == arg2) {
                        if (table[idx].attachSlot > 0 || cfg->weapon == table[idx].itemId - 0x7F) {
                            arg1--;
                        }
                        break;
                    }
                }
                for (i = 0; i < 3; i++) {
                    if (Gp_RelatedQty1.rows[table[idx].itemId - 0x80].related[i] == arg2) {
                        if (table[idx].attachSlot > 0 || cfg->weapon == table[idx].itemId - 0x7F) {
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

/// Empties the ammunition and attachment of weapon item `item`, the same clear
/// `Gp_ClearEquipSlot` performs.
static inline void _gpClearEquipSlot(s32 item)
{
    McItemSlot* slot;
    s32         found = 0;
    s32         i;

    if ((u32)(item - 0x80) >= 0x20) {
        return;
    }

    slot = &Mc_SaveData[0].weaponItems[item - 0x80];
    for (i = 0; i < 8; i++) {
        if (item == Gp_ItemMaps[i].field_1) {
            found = 1;
            break;
        }
    }

    if ((found == 0) || (Gp_ItemMaps[i].field_0 != 0)) {
        slot->ammoId  = 0;
        slot->ammoQty = 0;
    }

    if ((found == 0) || (Gp_ItemMaps[i].field_0 != 1)) {
        if (slot->attachId != 0xFF) {
            slot->attachId = 0;
        }
        slot->attachQty = 0;
    }
}

void Gp_RefreshItemRow(McItemRec* arg0)
{
    u8  item;
    s32 inRange;

    if (arg0->attachSlot <= 0) {
        return;
    }

    inRange          = (u8)(arg0->itemId + 0x80) < 0x20;
    arg0->attachSlot = 0;
    if (!inRange) {
        return;
    }

    item = arg0->itemId;
    if (item == Player_Status.weapon + 0x7F) {
        return;
    }

    _gpClearEquipSlot(item);
}

void func_800B92CC(Task* task)
{
    switch (GP_LOC_WORD(Mc_SaveData[0].at4.loc) & GP_LOC_STAGE_AREA) {
        case GP_LOC_KEY(1, 1, 0, 0):
            func_80180804();
            break;
        case GP_LOC_KEY(1, 15, 0, 0):
            func_8017EA68();
            break;
        case GP_LOC_KEY(1, 19, 0, 0):
            func_80181468();
            break;
        case GP_LOC_KEY(2, 1, 0, 0):
            func_8017EA90();
            break;
        case GP_LOC_KEY(2, 17, 0, 0):
            func_8017E9E8();
            break;
        case GP_LOC_KEY(2, 27, 0, 0):
            func_80181364();
            break;
        case GP_LOC_KEY(3, 1, 0, 0):
            func_8017E9F8();
            break;
        case GP_LOC_KEY(3, 17, 0, 0):
            func_8017EAE0();
            break;
        case GP_LOC_KEY(3, 27, 0, 0):
            func_8018138C();
            break;
        case GP_LOC_KEY(4, 6, 0, 0):
            func_8017EA78();
            break;
        case GP_LOC_KEY(4, 16, 0, 0):
            func_8017EB2C();
            break;
        case GP_LOC_KEY(4, 20, 0, 0):
            func_8017EDE8();
            break;
        case GP_LOC_KEY(4, 31, 0, 0):
            func_8017EAB4();
            break;
        case GP_LOC_KEY(4, 41, 0, 0):
            func_8017EA64();
            break;
        case GP_LOC_KEY(4, 47, 0, 0):
            func_8017EC04();
            break;
        case GP_LOC_KEY(5, 22, 0, 0):
            func_8017EAC4();
            break;
        case GP_LOC_KEY(5, 28, 0, 0):
            func_8017EA60();
            break;
        case GP_LOC_KEY(2, 30, 0, 0):
            func_8017EA58();
            break;
        case GP_LOC_KEY(3, 30, 0, 0):
            func_8017EA74();
            break;
    }
}

/// "Notice". The byte after the terminator is not zero: the original toolchain
/// left it in the alignment gap.
