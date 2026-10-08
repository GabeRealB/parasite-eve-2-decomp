/* The Neo Ark forest rooms' pool of roaming enemies. The room keeps up to five
 * reserve slots of banked HP, sized from game-flag nibbles per session slot:
 * each visit adds the slot's arming count (per location variant) to a running
 * total and caps it at five. The room's dormant placed actors (hp -999) are
 * revived from this pool. A 0x13EF request names a spawn point, and the room
 * gives the next dormant enemy a banked HP, raises a battle-state reference,
 * sends it the 0x7DB actor command and places it at that point with its yaw.
 * An enemy that retreats reports its HP through message 0x13F4. That HP goes
 * back into a free slot at 110%, capped at the enemy's maximum. When the
 * battle ends, the enemies still banked are folded back into the flag nibbles
 * so the count survives the room change. A frame cooldown spaces the arrivals.
 * Each room runs two such pools, one feeding area 0x1D (nibbles
 * 0x10C/0x10D/0x168) and one feeding area 0xB (nibbles 0x10A/0x10B/0x167).
 * They share the slots, cooldown and request state.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_ROAMING_ENEMIES_H
#define SRC_SHARED_ROAMING_ENEMIES_H

#include "types.h"

#include "gameplay/message.h"

/// Shared pool timing in per-frame state calls; zero enables requests and reset.
///
/// Positive values count down; -1 pauses until another pool action changes it.
enum {
    ROAMER_COOLDOWN_PAUSED             = -1,
    ROAMER_COOLDOWN_READY              = 0,
    ROAMER_INITIAL_COOLDOWN_FRAMES     = 30,
    ROAMER_ACTION_COOLDOWN_FRAMES      = 90,
    ROAMER_POST_BATTLE_COOLDOWN_FRAMES = 150,
};

/// No pending one-based spawn-point selector.
enum {
    ROAMER_SPAWN_POINT_NONE = 0,
};

/// A fixed point at which a pool places an enemy it revives.
///
/// A room keeps one table of these for each pool. A spawn request names a row
/// by its one-based position, and the pool's per-frame state then moves the
/// enemy's root coordinate to the row's `x` and `z` at height zero and
/// overwrites its rotation with `yaw`. Each table ends with a row's worth of
/// 0x7FFF halfwords, which the pools never read.
typedef struct {
    s16 x;   // World X the enemy's root coordinate is moved to
    s16 y;   // Zero in every row and never read: the pools place an enemy at height zero themselves
    s16 z;   // World Z the enemy's root coordinate is moved to
    s16 yaw; // Facing about the vertical axis, signed, 4096 units per turn
} RoamerSpawnPoint;
STATIC_ASSERT_SIZEOF(RoamerSpawnPoint, 8);

static void _roamerTickPoolA(Task* task);

#endif /* SRC_SHARED_ROAMING_ENEMIES_H */
