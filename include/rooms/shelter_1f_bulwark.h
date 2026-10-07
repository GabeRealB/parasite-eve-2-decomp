#ifndef INCLUDE_ROOMS_SHELTER_1F_BULWARK_H
#define INCLUDE_ROOMS_SHELTER_1F_BULWARK_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_shelter_1f_bulwark_80180DA8[12];

// shelter_1f_bulwark
extern WorldCollisionRoomResources D_shelter_1f_bulwark_801803B0[];

extern WorldCoordRoomLighting D_shelter_1f_bulwark_801803C0[];

extern u8* D_shelter_1f_bulwark_801803C8[];

extern ViewCount D_shelter_1f_bulwark_801803CC[];

extern DirectionWarpEntry D_shelter_1f_bulwark_801803D0[];

extern ViewCamera D_shelter_1f_bulwark_8018066C[];

extern SpriteView D_shelter_1f_bulwark_801807B0[];

extern WorldCollisionSurfaceProperties* D_shelter_1f_bulwark_80180E9C[];

void func_shelter_1f_bulwark_8017DC20(Task* task);

/// Runs the Bulwark's charging pink flash, peak screen tint and fading star.
///
/// Requires a counted effect with a coordinate body, owned zeroed `EffectWork`
/// in `spawnArg2.pointer` and initial state 0, as supplied by `effectSpawn`.
/// `spawnArg1.value` is a positive charge duration in running updates, consumed
/// as a countdown after initialization. Controls 1..3 pause without drawing;
/// control 4 and above cancels. A running update in state 3 or fade completion
/// also releases the work, task and body and decrements the effect count. Keep
/// the room overlay loaded, with current view transforms, scratch space and
/// frame packet storage.
void shelter1fBulwarkRoomVisualEffectsFlashTask(Task* task);

/// Records and draws the Bulwark's fading twin trails from two anchor-relative endpoints.
///
/// Requires a counted effect with a coordinate body, owned `EffectWork` in
/// `spawnArg2.pointer`, age 0, initial state 0 and null `Task::work`, as supplied
/// by `effectSpawn`.
/// The work's borrowed parent coordinate must remain live. Initialization
/// allocates two eight-coordinate histories in `Task::work`; allocation failure
/// resets age for a later retry. Each later update records both endpoints and
/// draws seven fading quads from their world-space snapshots.
///
/// `spawnArg1.value` is 0 for external teardown, or 2..32767 for release at that
/// active-update age; initialization occupies age 1. Age is a signed halfword
/// and wraps. Controls below 2 advance and draw; other controls retain the task
/// without drawing or cancellation. Teardown frees both histories, the effect
/// work and the coordinate body and decrements the effect count. Keep the room
/// overlay loaded, with current view transforms, scratch and frame packet space.
void shelter1fBulwarkRoomVisualEffectsTwinTrailTask(Task* task);

void func_shelter_1f_bulwark_8017F6D8(Task* task);

/// Draws the Bulwark's fixed glows and installs its room-specific combat effects.
///
/// State 0 selects the flash, twin-trail and spark-burst effect IDs, then enters
/// state 1. Drawing starts on that same update: mapped view 2 has four glows,
/// view 3 has one, and other views draw none. The radius inputs are world units;
/// red, green and blue intensity factors multiply the frame-parity pulse.
/// Spawn arguments and the task's coordinate body are unused. Keep the room
/// overlay loaded, with a current view matrix, scratch and frame packet space.
void shelter1fBulwarkDrawGlowsTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_1F_BULWARK_H
