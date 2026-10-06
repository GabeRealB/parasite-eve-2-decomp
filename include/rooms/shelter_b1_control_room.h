#ifndef INCLUDE_ROOMS_SHELTER_B1_CONTROL_ROOM_H
#define INCLUDE_ROOMS_SHELTER_B1_CONTROL_ROOM_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern TaskDesc D_shelter_b1_control_room_80181BBC[2];

extern AreaApplyRec D_shelter_b1_control_room_80183BE0[2];

extern TaskDesc D_shelter_b1_control_room_80181B88;

extern AreaVariant D_shelter_b1_control_room_80183A98[22];

// shelter_b1_control_room
extern u8* D_shelter_b1_control_room_80181C70[];

extern ViewCount D_shelter_b1_control_room_80181C74[];

extern DirectionWarpEntry D_shelter_b1_control_room_80181C78[];

extern WorldCollisionGrid D_shelter_b1_control_room_801820F8;

extern ViewCamera D_shelter_b1_control_room_8018211C[];

extern SpriteView D_shelter_b1_control_room_801833BC[];

extern WorldCoordRoomLights D_shelter_b1_control_room_801834DC;

extern WorldCollisionTrigger D_shelter_b1_control_room_801834F4[];

extern WorldCollisionTrigger D_shelter_b1_control_room_80183624[];

extern WorldCoordRoomAmbientEntry D_shelter_b1_control_room_80183B48[];

extern WorldCollisionSurfaceProperties* D_shelter_b1_control_room_80183BC0[];

void func_shelter_b1_control_room_8017EECC(Task* task);

/// Draws the control room's fixed glows for the current mapped camera view.
///
/// On state 0, installs this room's glow-disc, flying-spark and orange-burst
/// effect IDs, sets state 1 and draws in the same tick. Mapped views 2 and 3
/// draw six capsules and an orange disc; view 3 adds four blue discs. Views 4
/// and 6 draw three and four blue discs respectively; other views draw none.
/// Requires the room overlay and view mapping to remain loaded, the current
/// view matrix composed, and the frame's scratch stack and GPU arena ready.
/// Uses only `task->state`; keeps running until its owner removes the task.
void shelterB1ControlRoomDrawGlowsTask(Task* task);

void func_shelter_b1_control_room_8017FF80(Task* arg0);

/// Runs the control room's animated spark toward its initial target position.
///
/// Requires live `gRoomEffectState`, a counted effect task with a coordinate
/// body, initial state zero, and owned `EffectWork` in `spawnArg2.pointer` with
/// age and frame index zero.
/// `spawnArg1.pointer` borrows a target `GfxCoord` through the first running
/// update; both cached matrices must be current in the same view space.
/// That update fixes a parent-space step at 204/4096 of the initial separation,
/// with signed 16-bit narrowing before scaling. Later running updates move by
/// that step and draw on odd ages; age 20 releases the work and tears down the
/// task. Room effect control 1..3 pauses; 4 or above cancels. The target is not
/// sampled again. The control room overlay must remain loaded while dispatched.
void shelterB1ControlRoomRoomVisualEffectsFlyingSparkTask(Task* task);

/// Runs the control room's orange burst with an expanding glow and fading ring.
///
/// Requires live `gRoomEffectState`, a counted effect task with a coordinate
/// body, initial state zero, and owned `EffectWork` in `spawnArg2.pointer`;
/// `spawnArg1` is ignored.
/// Running updates compose the coordinate, grow the disc and layered glow,
/// and fade the ring before fading the disc. A successful ground projection
/// adds a ground quad; the glow refreshes a flickering orange point light even
/// when off screen. Room effect control 1..3
/// pauses; 4 or above cancels. Cancellation or completed fading releases the
/// work and tears down the task; callers must not retain released pointers.
/// The control room overlay must remain loaded while dispatched.
void shelterB1ControlRoomRoomVisualEffectsFlyingOrangeBurstTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B1_CONTROL_ROOM_H
