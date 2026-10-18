/* The sprites of the M4A1's Pyke flamethrower attachment, also carried by
 * actor_800100's copy of the weapon. One is the flame at the nozzle: a six-
 * cell animated strip billboarded at a world point. The other is the flying
 * flame: a spinning, widening billboard that the flame's task draws each
 * frame as it flies, falls and ricochets, with a splash on the ground below.
 *
 * Include this header in the prologue and flamethrower_flame.inc.c where the nozzle
 * draw begins; the fragment holds the nozzle draw, the flying flame's task,
 * its blob and splash draws and its release, in that order. The task is an
 * entry point gameplay's effect table names by address, so each package
 * exports `_flamethrowerFlameTask` under its own name with DEFINE_ALIAS. The two
 * builds differ in the flame's collision key, an enumerator
 * FLAMETHROWER_FLAME_ATTACK_KEY each carrier defines (from FLAMETHROWER_FLAME_KEY),
 * and in two code selections, 0/1 macros defined before the inclusion and
 * undefined at its end:
 *   FLAMETHROWER_FLAME_REDRAW_UPDATES_COORD   nonrunning-redraw cache policy
 *   FLAMETHROWER_FLAME_SPLASH_INCREMENT_BEFORE_DEPTH_STORE
 *                                             splash increment/store order
 */

#ifndef SRC_SHARED_PROJECTILES_FLAMETHROWER_FLAME_H
#define SRC_SHARED_PROJECTILES_FLAMETHROWER_FLAME_H

#include "common.h"
#include "types.h"

#include "gameplay/world_collision_types.h"
#include "main/task_types.h"

/// Sizing numerator for the nozzle billboard; projected half-side is 31 * scale / depth.
enum { FLAMETHROWER_FLAME_NOZZLE_SIZE_SCALE = 0x80 };

/// Fields of the category-2 weapon key shared by player and companion flames.
///
/// Attack row 30 selects Pyke damage, reaction, hit effect and cooldown;
/// distance row 28 independently selects distance and critical-hit scaling.
/// The companion bit disables player-only weapon damage bonuses. Bit 15 stays
/// clear, selecting weapon attack rows rather than Parasite Energy rows.
enum {
    FLAMETHROWER_FLAME_ATTACK_ROW         = 30,
    FLAMETHROWER_FLAME_DISTANCE_ROW       = 28,
    FLAMETHROWER_FLAME_DISTANCE_ROW_SHIFT = 8,
    FLAMETHROWER_FLAME_COMPANION_BIT      = 0x80,
    /// The player's flame key: a category-2 weapon attack, attack row 30, distance/critical row 28.
    FLAMETHROWER_FLAME_KEY = WORLD_COLLISION_CONTACT_ATTACK | (FLAMETHROWER_FLAME_DISTANCE_ROW << FLAMETHROWER_FLAME_DISTANCE_ROW_SHIFT) | FLAMETHROWER_FLAME_ATTACK_ROW,
};

static void _flamethrowerFlameDrawNozzle(const VECTOR3* worldPosition, u16 animationFrame, u16 sizeScale);
static void _flamethrowerFlameDrawBlob(const VECTOR3* worldPosition, u16 animationFrame, u16 sizeScale, s16 spinAngle);

/// Collision block of one flying flame, allocated on its first tick and kept
/// at `Task::work`.
///
/// The sphere is linked on collision list 1. Its radius is half the width
/// seeded at spawn; later growth of the drawn flame leaves that radius
/// unchanged. Its packed key is the carrier's `FLAMETHROWER_FLAME_ATTACK_KEY` (contact
/// category 2), and its centre stays the origin of the coordinate the flame
/// flies on. The sphere takes pair tests. Room geometry is a separate segment
/// test along the flight. An occupied contact whose key has category 3 in the
/// high halfword (0x30000) ends the flame. The exit callback unlinks `body`
/// before default task teardown frees this block.
typedef struct {
    WorldCollisionBody    body;        // Sphere linked on list 1; pair tests are enabled after the link
    WorldCollisionContact contacts[1]; // One-entry table `body` borrows. The entry is marked LAST; occupied contacts are cleared each flight frame
} FlamethrowerFlameBody;
STATIC_ASSERT_SIZEOF(FlamethrowerFlameBody, 0x38);

static void _flamethrowerFlameDrawSplash(const VECTOR3* worldPosition, s32 halfSize);
static void _flamethrowerFlameRelease(Task* task);

#endif /* SRC_SHARED_PROJECTILES_FLAMETHROWER_FLAME_H */
