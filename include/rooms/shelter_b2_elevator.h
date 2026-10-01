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

extern GpAreaApplyRec D_shelter_b2_elevator_8017E9F8[2];

extern GpAreaVariant D_shelter_b2_elevator_8017E964[11];

// shelter_b2_elevator
extern u8* D_shelter_b2_elevator_8017DFD8[];

extern GpViewCountRec D_shelter_b2_elevator_8017DFDC[];

extern GpWarpRec D_shelter_b2_elevator_8017DFE0[];

extern WorldCollisionGrid D_shelter_b2_elevator_8017E0E4;

extern GpViewRec D_shelter_b2_elevator_8017E108[];

extern SpriteView D_shelter_b2_elevator_8017E7BC[];

extern WorldCoordRoomLights D_shelter_b2_elevator_8017E840;

extern WorldCollisionTrigger D_shelter_b2_elevator_8017E858[];

extern WorldCollisionTrigger D_shelter_b2_elevator_8017E8F0[];

extern WorldCollisionSurfaceProperties* D_shelter_b2_elevator_8017E9D8[];

void func_shelter_b2_elevator_8017DB18(Task* task);

void func_shelter_b2_elevator_8017DB70(Task* unused);

#endif // INCLUDE_ROOMS_SHELTER_B2_ELEVATOR_H
