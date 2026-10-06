#ifndef INCLUDE_ROOMS_NEO_ARK_GARDEN_H
#define INCLUDE_ROOMS_NEO_ARK_GARDEN_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern TaskDesc D_neo_ark_garden_80181398;

extern AreaVariant D_neo_ark_garden_80182B54[13];

// neo_ark_garden
extern WorldCollisionRoomResources D_neo_ark_garden_8018140C[];

extern WorldCoordRoomLighting D_neo_ark_garden_8018141C[];

extern u8* D_neo_ark_garden_80181424[];

extern ViewCount D_neo_ark_garden_80181428[];

extern DirectionWarpEntry D_neo_ark_garden_8018142C[];

extern ViewCamera D_neo_ark_garden_801816E8[];

extern SpriteView D_neo_ark_garden_80182540[];

extern WorldCollisionSurfaceProperties* D_neo_ark_garden_80182BD8[];

void func_neo_ark_garden_8017EA9C(Task* task);

/// Runs the garden's animated spark along a fixed step toward its initial target.
///
/// Requires a counted effect task with a coordinate body, owned `EffectWork`
/// in `spawnArg2.pointer`, state zero and age/frame index zero. `spawnArg1.pointer`
/// borrows a target `GfxCoord` through the first running update, when both world
/// matrices must be composed. That update fixes a parent-space step at 204/4096
/// of the initial displacement, with intermediate signed 16-bit narrowing.
/// Later running updates move by that step and draw on odd ages; at age 20 the
/// work and task are released. Non-running room control pauses without drawing;
/// cancellation releases the effect. The target is not sampled again.
void neoArkGardenRoomVisualEffectsFlyingSparkTask(Task* task);

void func_neo_ark_garden_8017F790(Task* arg0);

/// Runs the garden's orange burst with a growing disc, glow and fading ring.
///
/// Requires a counted effect task with a coordinate body, owned `EffectWork`
/// in `spawnArg2.pointer` and initial state zero; `spawnArg1` is ignored. Running
/// updates compose the coordinate and expand the glow, fading the ring before
/// the central disc. The glow also refreshes an orange transient point light.
/// Non-running room control pauses without drawing. Cancellation or completed
/// fading releases the work and task; callers must not retain released pointers.
void neoArkGardenRoomVisualEffectsFlyingOrangeBurstTask(Task* task);

void func_neo_ark_garden_8017EA44(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_GARDEN_H
