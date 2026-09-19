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

/// Where the player is standing and which way they face: the player actor's
/// root coordinate, kept so a room entry can put them back where they were
/// rather than at the room's own start.
///
/// `PlayerStatus` carries it into a memory-card save, so a game resumed from a
/// card re-enters its room at the spot the player saved at.
typedef struct {
    s16 x;   // World X of the player actor's root coordinate
    s16 y;   // World Y, likewise
    s16 z;   // World Z, likewise
    s16 yaw; // Facing about Y, a 16-bit angle wrapped into [-0x800, 0x800)
} PlayerPos;
STATIC_ASSERT_SIZEOF(PlayerPos, 0x8);

/// The player character: position, health, energy and equipment.
///
/// Every overlay family reads this, so it is the one block of game state that
/// is always resident rather than belonging to a stage or an actor.
typedef struct _PlayerStatus {
    byte      unknown_0[4];
    MATRIX*   coordMtx;       // The player actor's coordinate matrix
    s32       exp;            // Experience available to spend on Parasite Energy levels
    s32       bp;             // Bounty points, the currency shops charge in
    PlayerPos pos;            // Position and facing, as carried into the save
    s16       hp;             // Current health (clamped down to hpMax)
    s16       hpMax;          // Maximum health (level base + training + armour, capped at 250)
    s16       mp;             // Current Parasite Energy (clamped down to mpMax)
    s16       mpMax;          // Maximum Parasite Energy (capped at 250)
    u8        field_20;       // Zeroed at init; nothing else in the decompiled C touches it
    u8        weapon;         // Equipped weapon (itemId - 0x7F, 0=none)
    u8        weaponSlotItem; // Item in the equipped weapon's slot (item + 0x61, 0=empty)
    u8        armor;          // Equipped armour (itemId - 0x5F, 0=none), contributes to hpMax
    u8        field_24;       // Set for one player mode/state combination; movement clears it, the HUD reads it
    u8        peStateFlags;   // Eight independent timed states, one per bit, each with its own countdown
    u8        field_26;       // Selects the animation, model and file set (1..4), meanings unproven
    byte      unknown_27[0x19];
    u8        field_40[0x40]; // Filled with 0xFF at init; no reader in the decompiled C
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
