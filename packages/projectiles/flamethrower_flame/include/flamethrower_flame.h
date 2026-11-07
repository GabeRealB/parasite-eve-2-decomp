/* The flame of the M4A1's Pyke flamethrower, also in actor_800100's copy of the weapon: the flame at the nozzle, a
 * six-cell animated strip billboarded at a point, and the flying flame, a spinning, widening billboard that flies,
 * falls and ricochets, splashing on the ground below.
 *
 * flamethrower_flame.inc.c holds the nozzle draw, the flying flame's task, its draws and its release; gameplay's effect
 * table names the task by address, so each carrier exports `_flamethrowerFlameTask` under its own name with
 * DEFINE_ALIAS. Before including it the carrier defines the enumerator FLAMETHROWER_FLAME_ATTACK_KEY, the flame's
 * collision key, and may set two 0/1 selections (default 0), undefined at the fragment's end:
 *   FLAMETHROWER_FLAME_REDRAW_UPDATES_COORD                whether a paused or hidden flame's redraw recomposes it
 *   FLAMETHROWER_FLAME_SPLASH_INCREMENT_BEFORE_DEPTH_STORE  whether the splash biases its depth before storing it
 */

#ifndef PROJECTILES_FLAMETHROWER_FLAME_H
#define PROJECTILES_FLAMETHROWER_FLAME_H

#include <core/pe2.h>

#include <gameplay/world_collision_types.h>
#include <engine/task_types.h>

/// Size of the nozzle billboard: its projected half-side is 31 * scale / depth.
enum { FLAMETHROWER_FLAME_NOZZLE_SIZE_SCALE = 0x80 };

/// Fields of the flame's collision key, a weapon attack (category 2): attack row 30 is Pyke's damage, reaction, hit
/// effect and cooldown, distance row 28 its distance and critical-hit scaling. The companion bit, set on
/// actor_800100's flame, drops the player's weapon damage bonuses.
enum {
    FLAMETHROWER_FLAME_ATTACK_ROW         = 30,
    FLAMETHROWER_FLAME_DISTANCE_ROW       = 28,
    FLAMETHROWER_FLAME_DISTANCE_ROW_SHIFT = 8,
    FLAMETHROWER_FLAME_COMPANION_BIT      = 0x80,
    /// The player's flame key.
    FLAMETHROWER_FLAME_KEY = WORLD_COLLISION_CONTACT_ATTACK | (FLAMETHROWER_FLAME_DISTANCE_ROW << FLAMETHROWER_FLAME_DISTANCE_ROW_SHIFT) | FLAMETHROWER_FLAME_ATTACK_ROW,
};

static void _flamethrowerFlameDrawNozzle(const VECTOR3* worldPosition, u16 animationFrame, u16 sizeScale);
static void _flamethrowerFlameDrawBlob(const VECTOR3* worldPosition, u16 animationFrame, u16 sizeScale, s16 spinAngle);

/// Collision of one flying flame, at `Task::work`: a sphere of half the flame's spawn width keyed with the carrier's
/// `FLAMETHROWER_FLAME_ATTACK_KEY`. A contact of category 3 ends the flame; room geometry is tested apart.
typedef struct {
    WorldCollisionBody    body;        // The flame's sphere, on collision list 1
    WorldCollisionContact contacts[1]; // `body`'s contacts, cleared each flight frame
} FlamethrowerFlameBody;
STATIC_ASSERT_SIZEOF(FlamethrowerFlameBody, 0x38);

static void _flamethrowerFlameDrawSplash(const VECTOR3* worldPosition, s32 halfSize);
static void _flamethrowerFlameRelease(Task* task);

#endif /* PROJECTILES_FLAMETHROWER_FLAME_H */
