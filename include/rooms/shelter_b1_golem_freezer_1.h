#ifndef INCLUDE_ROOMS_SHELTER_B1_GOLEM_FREEZER_1_H
#define INCLUDE_ROOMS_SHELTER_B1_GOLEM_FREEZER_1_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_shelter_b1_golem_freezer_1_8017F17C[23];

// shelter_b1_golem_freezer_1
extern u8* D_shelter_b1_golem_freezer_1_8017E790[];

extern ViewCount D_shelter_b1_golem_freezer_1_8017E794[];

extern DirectionWarpEntry D_shelter_b1_golem_freezer_1_8017E798[];

extern WorldCollisionGrid D_shelter_b1_golem_freezer_1_8017E9C0;

extern ViewCamera D_shelter_b1_golem_freezer_1_8017E9E4[];

extern SpriteView D_shelter_b1_golem_freezer_1_8017EDB0[];

extern WorldCoordRoomLights D_shelter_b1_golem_freezer_1_8017EE64;

extern WorldCollisionTrigger D_shelter_b1_golem_freezer_1_8017EE7C[];

extern WorldCollisionTrigger D_shelter_b1_golem_freezer_1_8017EFAC[];

extern WorldCoordRoomAmbientEntry D_shelter_b1_golem_freezer_1_8017F234[];

extern WorldCollisionSurfaceProperties* D_shelter_b1_golem_freezer_1_8017F290[];

/// Runs the freezer room task's setup, message-waiting or teardown state.
///
/// Requires loaded room resources and a valid state (0 setup, 1 idle, 2 teardown).
/// Setup publishes the room receiver and prepares the meeting actor's obstacle.
/// The map overlay spawns it without a body or work block.
void shelterB1GolemFreezer1RoomTask(Task* task);

/// Animates and drifts one ten-frame floor-mist puff, then releases its counted effect.
///
/// Requires a coordinate body, task state zero, and an owned, counted
/// `EffectWork` with frame index zero in `spawnArg2.pointer`, as supplied by
/// `effectSpawn`. `spawnArg1.value` packs
/// size in bits 0..11, ticks per animation cell in bits 12..14 (1 if bits
/// 12..15 are all zero), and drift speed in bits 16..23 (64 if zero), in local
/// coordinate units per task tick. Bit 15 alone would decode a zero period;
/// callers must supply a nonzero period whenever any of bits 12..15 is set.
///
/// Chooses a fixed random screen rotation and local-XZ drift on the first tick.
/// Draws before moving; cell zero advances on that tick, and later cells last
/// the requested period. The composed coordinate must be refreshed between
/// ticks. The work allocation and task retire after cell nine; cancellation
/// uses the effect spawner's exit callback. Keep this room overlay loaded for
/// the effect's lifetime.
void shelterB1GolemFreezer1FloorMistTask(Task* task);

/// Emits floor mist every fourth animation frame and draws the current view's glows.
///
/// Requires the freezer room resources, view and effect controller to be live.
/// Emits one puff at each of nine floor origins, with radius 64..1024 in steps
/// of 64 and Y in -255..0 game units. Each puff snapshots its position during
/// spawning; its size is 1024..1279, cell period five or six ticks and drift
/// speed eight units per tick. Glows are drawn in mapped views 2..5.
/// The task and spawn arguments are unused; room teardown owns its lifetime.
void shelterB1GolemFreezer1AmbientEffectsTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B1_GOLEM_FREEZER_1_H
