#ifndef INCLUDE_ROOMS_NEO_ARK_SHRINE_H
#define INCLUDE_ROOMS_NEO_ARK_SHRINE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_neo_ark_shrine_801866C8[13];

// neo_ark_shrine
extern WorldCollisionRoomResources D_neo_ark_shrine_80182724[];

extern WorldCoordRoomLighting D_neo_ark_shrine_80182784[];

extern u8* D_neo_ark_shrine_801827F0[];

extern ViewCount D_neo_ark_shrine_80182808[];

extern DirectionWarpEntry D_neo_ark_shrine_80182814[];

extern ViewCamera D_neo_ark_shrine_801836BC[];

extern SpriteView D_neo_ark_shrine_80185280[];

extern WorldCollisionSurfaceProperties* D_neo_ark_shrine_80186844[];

/// Draws the shrine's fixed light flares selected by the current mapped view.
///
/// The first tick also selects this room's flash, twin-trail and spark-burst
/// effect IDs. Subsequent ticks draw the same view-selected world positions.
/// Requires the view, packet arena, ordering table and scratch stack ready
/// for drawing. The task remains active until external teardown.
void neoArkShrineFlareTask(Task* task);

/// Runs the shrine's charging pink flash, peak screen tint and fading star.
///
/// Requires a coordinate body and counted, owned `EffectWork` in
/// `spawnArg2.pointer`. `spawnArg1.value` is a positive charge duration in
/// active ticks, consumed as a countdown. Nonzero room effect control pauses
/// the task; control 4 or above, state 3, or completion releases the effect.
void neoArkShrineRoomVisualEffectsFlashTask(Task* task);

/// Records two parent-relative endpoints and draws their fading twin trails.
///
/// Requires a coordinate body and counted, owned `EffectWork` in
/// `spawnArg2.pointer`, whose parent stays live until teardown. `Task::work`
/// starts null and owns one allocation holding two eight-coordinate histories. The signed
/// 16-bit age advances only with room effect control below 2, including the
/// initialization tick. Allocation failure resets age to zero for retry.
/// `spawnArg1.value` is the completion age (2..32767); zero prevents automatic
/// completion. Teardown releases both the history and effect allocations.
void neoArkShrineRoomVisualEffectsTwinTrailTask(Task* task);

void func_neo_ark_shrine_801811EC(Task* task);

/// Initializes the shrine's room messages and puzzle, then waits for room messages.
///
/// The map's room descriptor supplies a bodyless task starting in state 0.
/// State 0 registers it in `GAME_TASK_SLOT_ROOM` and resets the board; state 1
/// idles with the message table installed, and state 2 kills the task. The
/// dispatcher requires `Task::state` in 0..2 and does not check that bound.
void neoArkShrineRoomTask(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_SHRINE_H
