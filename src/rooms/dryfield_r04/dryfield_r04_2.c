#include "rooms/dryfield_r04.h"

#include "types.h"

#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

/* The records after Dryfield room 4's collision grid: its two views, their
 * empty sprite lists and the location's parameters.
 */

ViewCamera gDryfieldR04ViewCameras[2] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 0, 30000, 0 } }, 207 },
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -15000, 30000, -16000 } }, 207 },
};

/// Keeps decoded image strips without source sprites for mapped view 1.
///
/// A zero-count prefix selects the background; SPRITE_BATCH_END terminates
/// the list. The descriptor also retains this list in sources.empty, which
/// is never dereferenced as a SpriteSource pool.
static SpriteBatch _gDryfieldR04View1SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

/// Keeps decoded image strips without source sprites for mapped view 2.
///
/// A zero-count prefix selects the background; SPRITE_BATCH_END terminates
/// the list. The descriptor also retains this list in sources.empty, which
/// is never dereferenced as a SpriteSource pool.
static SpriteBatch _gDryfieldR04View2SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

SpriteView gDryfieldR04SpriteViews[2] = {
    { { .empty = _gDryfieldR04View1SpriteBatches }, _gDryfieldR04View1SpriteBatches, NULL },
    { { .empty = _gDryfieldR04View2SpriteBatches }, _gDryfieldR04View2SpriteBatches, NULL },
};

/// Three retained words with type-1 sound-script request values.
///
/// No C or assembly consumer establishes their role or subdivision. Their
/// layout fits `WorldCollisionFootstepSounds`, but the surface policy does not
/// reference them; a footstep interpretation remains unproven.
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
