#include "rooms/dryfield_r04.h"

#include "types.h"

#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

/* The records after Dryfield room 4's collision grid: its two views, their
 * empty sprite lists and the location's parameters.
 */

ViewCamera D_dryfield_r04_8017E218[2] = {
    { { { { 0x1000, 0, 0 }, { 0, 0, -0x1000 }, { 0, 0x1000, 0 } }, { 0, 0x7530, 0 } }, 0xCF },
    { { { { 0x1000, 0, 0 }, { 0, 0, -0x1000 }, { 0, 0x1000, 0 } }, { -0x3A98, 0x7530, -0x3E80 } }, 0xCF },
};

static SpriteBatch D_dryfield_r04_8017E260[2] = {
    { 0, 0, 0, 0, { 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0 } },
};

static SpriteBatch D_dryfield_r04_8017E270[2] = {
    { 0, 0, 0, 0, { 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0 } },
};

SpriteView D_dryfield_r04_8017E280[2] = {
    { { .empty = D_dryfield_r04_8017E260 }, D_dryfield_r04_8017E260, NULL },
    { { .empty = D_dryfield_r04_8017E270 }, D_dryfield_r04_8017E270, NULL },
};

/// Three sound IDs with the layout of `WorldCollisionFootstepSounds`.
/// The room's surface-property record does not reference them.
static s32 D_dryfield_r04_8017E298[3] = { 0x10000011, 0x10000013, 0x10000011 };

/// Collision policy for surface classes 0..7: probes block, impacts are enabled and pushback applies.
///
/// No footstep cues; the loaded room overlay owns this record.
static WorldCollisionSurfaceProperties _gDryfieldR04SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

WorldCollisionSurfaceProperties* gDryfieldR04SurfacePropertiesByClass[8] = {
    &_gDryfieldR04SurfaceProperties,
    &_gDryfieldR04SurfaceProperties,
    &_gDryfieldR04SurfaceProperties,
    &_gDryfieldR04SurfaceProperties,
    &_gDryfieldR04SurfaceProperties,
    &_gDryfieldR04SurfaceProperties,
    &_gDryfieldR04SurfaceProperties,
    &_gDryfieldR04SurfaceProperties,
};
