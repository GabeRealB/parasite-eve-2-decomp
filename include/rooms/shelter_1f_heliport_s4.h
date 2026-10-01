#ifndef INCLUDE_ROOMS_SHELTER_1F_HELIPORT_S4_H
#define INCLUDE_ROOMS_SHELTER_1F_HELIPORT_S4_H

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// shelter_1f_heliport_s4
extern u8* D_shelter_1f_heliport_s4_8017D6F8[];

extern GpViewCountRec D_shelter_1f_heliport_s4_8017D6FC[];

extern GpWarpRec D_shelter_1f_heliport_s4_8017D700[];

extern WorldCollisionGrid D_shelter_1f_heliport_s4_8017D9C8;

extern GpViewRec D_shelter_1f_heliport_s4_8017D9EC[];

extern GpSprtRec D_shelter_1f_heliport_s4_8017DAF0[];

extern WorldCoordRoomLights D_shelter_1f_heliport_s4_8017DE6C;

extern GpObj4A D_shelter_1f_heliport_s4_8017DE84[];

extern WorldCollisionSurfaceProperties* D_shelter_1f_heliport_s4_8017E060[];

void func_shelter_1f_heliport_s4_8017D678(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_1F_HELIPORT_S4_H
