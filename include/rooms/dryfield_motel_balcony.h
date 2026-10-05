#ifndef INCLUDE_ROOMS_DRYFIELD_MOTEL_BALCONY_H
#define INCLUDE_ROOMS_DRYFIELD_MOTEL_BALCONY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_motel_balcony_80186220[13];

// dryfield_motel_balcony
extern WorldCollisionRoomResources D_dryfield_motel_balcony_801822D0[];

extern u8* D_dryfield_motel_balcony_801822E0[];

extern ViewCount D_dryfield_motel_balcony_801822E4[];

extern WorldCoordRoomLighting D_dryfield_motel_balcony_801822E8[];

extern DirectionWarpEntry D_dryfield_motel_balcony_801822F0[];

extern ViewCamera D_dryfield_motel_balcony_80182B80[];

extern SpriteView D_dryfield_motel_balcony_80185C98[];

extern WorldCollisionSurfaceProperties* D_dryfield_motel_balcony_80186704[];

/// Runs this room's charging pink flash, peak screen tint and fading star.
///
/// Requires a coordinate body and a counted, owned `EffectWork` in
/// `spawnArg2.pointer`. `spawnArg1.value` is a positive charge duration in
/// active ticks, consumed as a countdown. Nonzero room effect control pauses
/// the task; control 4 or above, state 3, or completion releases the effect.
void dryfieldMotelBalconyRoomVisualEffectsFlashTask(Task* task);

/// Records two anchor-relative endpoints and draws their fading trail as a beam.
///
/// Requires a coordinate body and a counted, owned `EffectWork` in
/// `spawnArg2.pointer`, whose `parent` stays live until teardown. `work` starts
/// null and owns sixteen allocated coordinate frames, eight per endpoint.
/// `spawnArg1.value` is the nonzero signed-16-bit age at which to finish; zero
/// leaves lifetime to external teardown. Age advances only with room effect
/// control below 2, including the successful initialization tick; allocation
/// failure resets it for retry. Teardown releases both allocations.
void dryfieldMotelBalconyRoomVisualEffectsTwinTrailTask(Task* task);

void func_dryfield_motel_balcony_80181628(Task* task);

void func_dryfield_motel_balcony_8017DCB8(Task* task);

void func_dryfield_motel_balcony_8017EA00(Task* arg0);

/// Runs this room's expanding orange disc, layered glow and fading outer ring.
///
/// Requires a coordinate body and a counted, owned `EffectWork` in
/// `spawnArg2.pointer`; the spawn duration is unused. Nonzero room effect
/// control pauses the task; control 4 or above or completion releases it.
void dryfieldMotelBalconyRoomVisualEffectsHaloOrangeBurstTask(Task* task);

void func_dryfield_motel_balcony_801801A8(Task* arg0);

void func_dryfield_motel_balcony_8017DC28(Task* arg0);

void func_dryfield_motel_balcony_8017DBD0(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_MOTEL_BALCONY_H
