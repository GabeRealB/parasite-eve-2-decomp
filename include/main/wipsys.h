#ifndef WIPSYS_H
#define WIPSYS_H

#include "common.h"

#include <psyq/libgte.h>

// =============================================================================
// Types — provisional system config / flags (rename when roles are proven)
// =============================================================================

/// WIP: boot/gamemain flag block (Wip_SysFlags). field_4 set on soft-reset paths;
/// field_6 polled/cleared in boot and stream paths. Role not fully proven.
typedef struct _WipSysFlags {
    byte field_0;
    s8   field_1;
    byte unknown_2[2];
    s16  field_4;
    s16  field_6;
    byte unknown_8[0x18];
} WipSysFlags;
STATIC_ASSERT_SIZEOF(WipSysFlags, 0x20);

/// Player X/Y/Z (low 16 bits of `GsCOORDINATE2.coord.t`) plus wrapped yaw.
/// Nested at `Player_Status` 0x10; written by `Gp_SavePlayerPos`, which wraps
/// `yaw` into [-0x800, 0x800).
typedef struct _PlayerPos {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
    /* 0x4 */ s16 z;
    /* 0x6 */ s16 yaw;
} PlayerPos;
STATIC_ASSERT_SIZEOF(PlayerPos, 0x8);

/// Player character state (main BSS, 0x80). Position, HP/MP and equipped item
/// ids; read by every overlay family. `Player_InitNewGameStats` seeds a new
/// game with 100/100 HP and MP.
typedef struct _PlayerStatus {
    /* 0x00 */ byte      unknown_0[4];
    /* 0x04 */ MATRIX*   coordMtx; // &GsCOORDINATE2.coord of the player actor
    /* 0x08 */ s32       exp;      // spent to raise PE levels (`Gp_PeUpgradePanelTask`)
    /* 0x0C */ s32       bp;       // shop currency, drawn under the "BP" label
    /* 0x10 */ PlayerPos pos;
    /* 0x18 */ s16       hp;       // clamped down to hpMax by `Gp_RecalcMaxHp`
    /* 0x1A */ s16       hpMax;    // `Gp_StatRows[level]` + bonus + armour, capped at 250
    /* 0x1C */ s16       mp;       // clamped down to mpMax by `Gp_RecalcMaxMp`
    /* 0x1E */ s16       mpMax;    // recomputed by `Gp_RecalcMaxMp`, capped at 250
    /* 0x20 */ u8        field_20; // zeroed by Player_InitNewGameStats; role unproven
                                   /// Equipped weapon, stored as `itemId - 0x7F` (weapon ids are 0x80..0x9F).
                                   /// 0 means nothing equipped. Maintained by `Gp_EquipHeld`.
    /* 0x21 */ u8 weapon;
    /// Contents of the equipped weapon's slot (`Mc_SaveData.field_1C8[weapon +
    /// 0x7F]`), stored as `item + 0x61`. `Gp_SyncHeldRelated` keeps it in step
    /// with `weapon`; `func_801061F0` packs both into `GameActor.field_124`.
    /* 0x22 */ u8 weaponSlotItem;
    /// Equipped armour, stored as `itemId - 0x5F` (armour ids are 0x60..0x7F).
    /// `Gp_RecalcMaxHp` adds `Gp_ModStatAttrs[armor - 1].field_4` to `hpMax`.
    /* 0x23 */ u8 armor;
    /* 0x24 */ u8 field_24; // cleared by Gp_MoveActorBy, preserved by Gp_MoveActorByKeep; role unproven
                            /// Timed player-state bitmask. `Gp_TriggerPeState` sets a bit and arms the
                            /// matching GameActor countdown (field_944..field_950) at 0x258 frames; the
                            /// tick clears the bit when its timer expires.
    /* 0x25 */ u8 peStateFlags;
    /// Small 1..4 discriminator, cached in `Game_Session.field_11E`. Indexes
    /// player animation and model tables (`D_80112DFC[arg2 + field_26 - 2]`,
    /// `table[type * 4 + field_26 - 5]`), a 1-based CdCmd 0x21 file-id table,
    /// and `Task_Spawn(7, field_26 + 3, ...)`. Meaning of the values unproven.
    /* 0x26 */ u8   field_26;
    /* 0x27 */ byte unknown_27[0x19];
    /* 0x40 */ u8   field_40[0x40]; // filled 0xFF on new game; role unproven
} PlayerStatus;
STATIC_ASSERT_SIZEOF(PlayerStatus, 0x80);

// =============================================================================
// Globals
// =============================================================================

extern WipSysFlags  Wip_SysFlags;
extern PlayerStatus Player_Status;

// --- APIs ---
/// Seed Player_Status for a new game: 100/100 HP and MP, no equipment.
void Player_InitNewGameStats(void);

#endif // WIPSYS_H
