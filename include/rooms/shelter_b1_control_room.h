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

void func_shelter_b1_control_room_801804D8(Task* task);

void func_shelter_b1_control_room_80181138(Task* arg0);

#endif // INCLUDE_ROOMS_SHELTER_B1_CONTROL_ROOM_H
