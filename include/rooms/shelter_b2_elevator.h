#ifndef INCLUDE_ROOMS_SHELTER_B2_ELEVATOR_H
#define INCLUDE_ROOMS_SHELTER_B2_ELEVATOR_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaApplyRec D_shelter_b2_elevator_8017E9F8[2];

extern AreaVariant D_shelter_b2_elevator_8017E964[11];

// shelter_b2_elevator
extern u8* D_shelter_b2_elevator_8017DFD8[];

extern ViewCount D_shelter_b2_elevator_8017DFDC[];

extern DirectionWarpEntry D_shelter_b2_elevator_8017DFE0[];

extern WorldCollisionGrid D_shelter_b2_elevator_8017E0E4;

extern ViewCamera D_shelter_b2_elevator_8017E108[];

extern SpriteView D_shelter_b2_elevator_8017E7BC[];

extern WorldCoordRoomLights D_shelter_b2_elevator_8017E840;

extern WorldCollisionTrigger D_shelter_b2_elevator_8017E858[];

extern WorldCollisionTrigger D_shelter_b2_elevator_8017E8F0[];

extern WorldCollisionSurfaceProperties* D_shelter_b2_elevator_8017E9D8[];

/// Door motion messages accepted by the active Shelter B2 elevator room task.
///
/// Sent after both leaf tasks have spawned; payload words are ignored. Motion
/// begins in their next update, clamps at each end and leaves the request active.
enum {
    SHELTER_B2_ELEVATOR_MESSAGE_OPEN_DOOR  = 5100,
    SHELTER_B2_ELEVATOR_MESSAGE_CLOSE_DOOR = 5101,
};

/// Maintains the Shelter B2 elevator's room controller and entry sequence.
///
/// Requires a live task with state 0..2: initialize the room and its door leaves,
/// idle while receiving room messages, or tear down with `taskKill`.
/// Dispatch is unchecked; keep this room overlay loaded for the task's lifetime.
void shelterB2ElevatorRoomTask(Task* task);

/// No-op callback for the Shelter B2 elevator's room-effect slot 0x139.
///
/// Leaves the task, its coordinate body and its state unchanged. Normal task
/// teardown owns their release; this room overlay must remain loaded meanwhile.
void shelterB2ElevatorEffectNoopTask(Task* unusedTask);

#endif // INCLUDE_ROOMS_SHELTER_B2_ELEVATOR_H
