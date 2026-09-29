#ifndef GAMEPLAY_PRIVATE_INVENTORY_H
#define GAMEPLAY_PRIVATE_INVENTORY_H

#include "common.h"

#include "main/mc.h"
#include "main/mc_types.h"

/// 8-byte item attribute row in `Gp_ModStatAttrs`, indexed by item id minus
/// 0x60 (armor ids 0x60–0x7F).
/// `flags` is a bit set of abilities granted while an armor row's item is
/// equipped.
/// field_4 is the unsigned bonus added to `Player_Status.hpMax` by
/// `Gp_RecalcMaxHp` when `Player_Status.armor` (item id − 0x5F) is
/// non-zero. field_5 is the unsigned base added to
/// `Mc_SaveData[0].state.itemLevelBonus[id-0x60]` and clamped to 10. field_6 is the
/// unsigned bonus added to `Player_Status.mpMax` by `Gp_RecalcMaxMp`
/// when `field_23` is non-zero.
typedef struct _GpItemAttr {
    /* 0x00 */ s32 flags;
    /* 0x04 */ u8  field_4;
    /* 0x05 */ u8  field_5;
    /* 0x06 */ u8  field_6;
    /* 0x07 */ u8  field_7;
} GpItemAttr;
STATIC_ASSERT_SIZEOF(GpItemAttr, 0x8);

/// Cursor represented as either a row pointer or its 32-bit PS1 address.
typedef union GpItemRowAddress {
    McItemRec* row;
    u32        word;
} GpItemRowAddress;
STATIC_ASSERT_SIZEOF(GpItemRowAddress, 4);

/// Resolve a row in the PS1 inventory address space. Address words preserve
/// the runtime table base; row fields are always accessed through McItemRec.
static inline McItemRec* gpItemRowAt(McItemRec* rows, s32 index)
{
    GpItemRowAddress base;
    GpItemRowAddress result;
    base.row     = rows;
    result.word  = index * sizeof(McItemRec);
    result.word += base.word;
    return result.row;
}

/// Inline form of `Gp_GetItemSlot`: weapon `item`'s entry in the save's
/// per-weapon equipment table.
static inline McItemSlot* gpItemSlot(s32 item)
{
    return &Mc_SaveData[0].state.weaponItems[item - 0x80];
}

#endif // GAMEPLAY_PRIVATE_INVENTORY_H
