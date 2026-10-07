#ifndef INCLUDE_ROOMS_MINE_CAVERN_H
#define INCLUDE_ROOMS_MINE_CAVERN_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_mine_cavern_8018E238[22];

// mine_cavern
extern WorldCollisionRoomResources D_mine_cavern_80188FE0[];

extern WorldCoordRoomLighting D_mine_cavern_80189010[];

extern u8* D_mine_cavern_80189060[];

extern ViewCount D_mine_cavern_8018906C[];

extern DirectionWarpEntry D_mine_cavern_80189074[];

extern ViewCamera D_mine_cavern_80189840[];

extern SpriteView D_mine_cavern_8018CD10[];

extern WorldCollisionSurfaceProperties* D_mine_cavern_8018E30C[];

/// Runs the cavern's expanding orange disc, layered glow and fading ring effect.
///
/// Requires a coordinate-body task with owned `EffectWork` in
/// `spawnArg2.pointer`, as supplied by `effectSpawn`. Room effect control 0
/// runs it, 1..3 pause it, and 4 or above cancels it. Completion or cancellation
/// releases the effect work and task; its coordinate ancestors must stay live.
void mineCavernRoomVisualEffectsHaloOrangeBurstTask(Task* task);

void func_mine_cavern_8017E474(Task* arg0);

/// Runs the cavern's vertically drifting animated mote effect.
///
/// Requires a coordinate-body task and zeroed owned `EffectWork` in
/// `spawnArg2.pointer`. `spawnArg1` packs world-unit half-extent in bits 0..11
/// (bits 0..1 also select steady motion, bit 1 upward), palette in bits 12..15
/// (0 default), unsigned speed in bits 16..23 and signed lifetime in bits
/// 24..31; speed is local-Y units per active tick, lifetime is active ticks.
/// Room effect control 1..3 pauses it; 4 or above cancels it. Completion or
/// cancellation releases the work and task; coordinate ancestors must stay live.
void mineCavernRoomVisualEffectsMoteTask(Task* task);

/// Runs the cavern's expanding tinted halo and shrinking ring, then a fading star.
///
/// Requires a coordinate-body task and zeroed owned `EffectWork` in
/// `spawnArg2.pointer`, with a borrowed parent and local world-unit offset.
/// `spawnArg1`'s signed low half is a positive expansion duration in active
/// ticks; its signed high half selects tint row 0..2. Initialization replaces
/// the argument with the remaining duration. Room effect control 1..3 pauses
/// it; 4 or above cancels it. Completion or cancellation releases work and task;
/// the parent and its ancestors must stay live until then.
void mineCavernRoomVisualEffectsHaloTask(Task* task);

void func_mine_cavern_80181730(Task* arg0);

/// Runs the cavern's room controller: initialization, event polling, then teardown.
///
/// The live task's state must be 0..2, initially 0; dispatch copies the three
/// handlers by value and does not check bounds. Requires the cavern's scripts
/// and room resources to remain loaded, together with the live session and
/// gameplay APIs. State 2 releases the task.
void mineCavernRoomTask(Task* task);

#endif // INCLUDE_ROOMS_MINE_CAVERN_H
