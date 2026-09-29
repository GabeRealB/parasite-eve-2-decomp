#ifndef MAIN_WIPSYS_TYPES_H
#define MAIN_WIPSYS_TYPES_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

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

/// Captured player position and facing for restoring the player at room entry.
///
/// `PlayerStatus` retains this snapshot independently of the live actor and
/// includes it in memory-card saves. Coordinates use integer world units:
/// capture keeps their low 16 bits, and restore sign-extends them to 32 bits.
/// Yaw is taken from the root matrix's forward direction, with both half-turn
/// endpoints retained in the range [-2048, 2048].
typedef struct {
    s16 x;   // Captured root X position, in signed world units
    s16 y;   // Captured root Y position, in signed world units
    s16 z;   // Captured root Z position, in signed world units
    s16 yaw; // Captured facing about Y, 4096 units per turn, inclusive [-2048, 2048]
} PlayerPos;
STATIC_ASSERT_SIZEOF(PlayerPos, 0x8);

/// The player character: position, health, energy and equipment, plus the
/// memory-card backup of that 0x40-byte record. Mc_BufferSlots[2] copies,
/// checksums and compares both halves during save/load.
///
/// Every overlay family reads this, so it is the one block of game state that
/// is always resident rather than belonging to a stage or an actor.
typedef struct _PlayerStatus {
    u16       checksum;         // Signed-byte sum of the current record after this header
    u16       checksumComplement;
    MATRIX*   coordMtx;         // The player actor's coordinate matrix
    s32       exp;              // Experience available to spend on Parasite Energy levels
    s32       bp;               // Bounty points, the currency shops charge in
    PlayerPos pos;              // Position and facing, as carried into the save
    s16       hp;               // Current health (clamped down to hpMax)
    s16       hpMax;            // Maximum health (level base + training + armour, capped at 250)
    s16       mp;               // Current Parasite Energy (clamped down to mpMax)
    s16       mpMax;            // Maximum Parasite Energy (capped at 250)
    u8        field_20;         // Zeroed at init; nothing else in the decompiled C touches it
    u8        weapon;           // Equipped weapon (itemId - 0x7F, 0=none)
    u8        weaponSlotItem;   // Item in the equipped weapon's slot (item + 0x61, 0=empty)
    u8        armor;            // Equipped armour (itemId - 0x5F, 0=none), contributes to hpMax
    u8        field_24;         // Set for one player mode/state combination; movement clears it, the HUD reads it
    u8        peStateFlags;     // Eight independent timed states, one per bit, each with its own countdown
    u8        field_26;         // Selects the animation, model and file set (1..4), meanings unproven
    byte      unknown_27[0x19];
    u8        saveBackup[0x40]; // Second checksummed copy of the first 0x40 bytes
} PlayerStatus;
STATIC_ASSERT_SIZEOF(PlayerStatus, 0x80);
STATIC_ASSERT(OFFSET_OF(PlayerStatus, saveBackup) == 0x40, player_status_backup_offset);

#endif // MAIN_WIPSYS_TYPES_H
