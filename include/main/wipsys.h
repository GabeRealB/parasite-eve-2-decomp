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

/// Where the player is standing and which way they face, as stored in the save.
typedef struct _PlayerPos {
    s16 x;
    s16 y;
    s16 z;

    /// Facing, wrapped into [-0x800, 0x800).
    s16 yaw;
} PlayerPos;
STATIC_ASSERT_SIZEOF(PlayerPos, 0x8);

/// The player character: position, health, energy and equipment.
///
/// Every overlay family reads this, so it is the one block of game state that
/// is always resident rather than belonging to a stage or an actor.
typedef struct _PlayerStatus {
    byte unknown_0[4];

    /// Points at the player actor's coordinate matrix.
    MATRIX* coordMtx;

    /// Experience available to spend raising Parasite Energy levels.
    s32 exp;

    /// Bounty points, the currency shops charge in.
    s32 bp;

    /// Position and facing, carried into the save file.
    PlayerPos pos;

    /// Current health. Recomputing the maximum clamps this down to it.
    s16 hp;

    /// Maximum health: the level's base value plus bonuses from training and
    /// armour, capped at 250.
    s16 hpMax;

    /// Current Parasite Energy. Recomputing the maximum clamps this down to it.
    s16 mp;

    /// Maximum Parasite Energy, capped at 250.
    s16 mpMax;

    /// Zeroed when a new game starts. Role unproven.
    u8 field_20;

    /// Equipped weapon as `itemId - 0x7F`, or 0 when nothing is equipped.
    u8 weapon;

    /// The item loaded into the equipped weapon's slot, as `item + 0x61`. Kept
    /// in step with `weapon`, and packed together with it into the player
    /// actor's model descriptor.
    u8 weaponSlotItem;

    /// Equipped armour as `itemId - 0x5F`, or 0 when nothing is equipped.
    /// Contributes to `hpMax`.
    u8 armor;

    /// Cleared when the actor is moved, preserved by the variant that keeps it.
    /// Role unproven.
    u8 field_24;

    /// Timed player states, one per bit. Setting a bit arms a countdown on the
    /// player actor; the bit clears when that timer expires.
    u8 peStateFlags;

    /// Selects which animation, model and file set the player uses. Takes the
    /// values 1 to 4; what each one means is unproven.
    u8 field_26;

    byte unknown_27[0x19];

    /// Filled with 0xFF when a new game starts. Role unproven.
    u8 field_40[0x40];
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
