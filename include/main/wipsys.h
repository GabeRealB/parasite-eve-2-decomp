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
/// Nested at `Player_Status` 0x10 (`Gp_SavePlayerPos`).
typedef struct _PlayerPos {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
    /* 0x4 */ s16 field_4;
    /* 0x6 */ s16 field_6;
} PlayerPos;
STATIC_ASSERT_SIZEOF(PlayerPos, 0x8);

/// Player character state (main BSS, 0x80). Position, HP/MP and equipped item
/// ids; read by every overlay family. `Player_InitNewGameStats` seeds a new
/// game with 100/100 HP and MP; field_40 is filled 0xFF by
/// `Mc_InitDualBankBuffers`.
typedef struct _PlayerStatus {
    /* 0x00 */ byte      unknown_0[4];
    /* 0x04 */ MATRIX*   field_4;  // MATRIX* of actor GsCOORDINATE2.coord (`Gp_InitPlayerWork`)
    /* 0x08 */ s32       field_8;
    /* 0x0C */ s32       field_C;  // copied to Mc_SaveData.field_18 (`Gp_SavePlayerPos`)
    /* 0x10 */ PlayerPos field_10;
    /* 0x18 */ s16       hp;       // clamped down to hpMax by `Gp_RecalcMaxHp`
    /* 0x1A */ s16       hpMax;    // `Gp_StatRows[level]` + bonus + armour, capped at 250
    /* 0x1C */ s16       mp;       // clamped down to mpMax by `Gp_RecalcMaxMp`
    /* 0x1E */ s16       mpMax;    // recomputed by `Gp_RecalcMaxMp`, capped at 250
    /* 0x20 */ u8        field_20;
    /* 0x21 */ u8        field_21; // selected item id - 0x7F (`Gp_EquipHeld`)
    /* 0x22 */ u8        field_22; // packed into GameActor.field_124 bits 0-7 (`func_801061F0`)
    /* 0x23 */ u8        field_23; // selected item id - 0x5F (`func_800CEC5C`)
    /* 0x24 */ u8        field_24; // cleared/set by func_80104A4C; cleared by Gp_MoveActorBy; preserved by Gp_MoveActorByKeep
    /* 0x25 */ u8        field_25;
    /* 0x26 */ u8        field_26;
    /* 0x27 */ byte      unknown_27[0x19];
    /* 0x40 */ u8        field_40[0x40];
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
