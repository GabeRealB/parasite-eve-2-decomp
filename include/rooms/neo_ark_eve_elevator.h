#ifndef INCLUDE_ROOMS_NEO_ARK_EVE_ELEVATOR_H
#define INCLUDE_ROOMS_NEO_ARK_EVE_ELEVATOR_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// neo_ark_eve_elevator
extern GpRoomObjRec D_neo_ark_eve_elevator_8017D74C[];

extern WorldCoordRoomLighting D_neo_ark_eve_elevator_8017D75C[];

extern u8* D_neo_ark_eve_elevator_8017D764[];

extern GpViewCountRec D_neo_ark_eve_elevator_8017D768[];

extern GpWarpRec D_neo_ark_eve_elevator_8017D76C[];

extern GpViewRec D_neo_ark_eve_elevator_8017DA50[];

extern GpSprtRec D_neo_ark_eve_elevator_8017DB20[];

extern WorldCollisionSurfaceProperties* D_neo_ark_eve_elevator_8017DC30[];

void func_neo_ark_eve_elevator_8017D71C(Task* unused);

void func_neo_ark_eve_elevator_8017D6C4(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_EVE_ELEVATOR_H
