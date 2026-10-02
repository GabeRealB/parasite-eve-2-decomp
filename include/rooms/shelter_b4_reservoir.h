#ifndef INCLUDE_ROOMS_SHELTER_B4_RESERVOIR_H
#define INCLUDE_ROOMS_SHELTER_B4_RESERVOIR_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// Room data exported to the shared waypoint actor.
extern SVECTOR* D_shelter_b4_reservoir_801851D4[4];

extern SVECTOR D_shelter_b4_reservoir_801851E4[5];

extern s16 D_shelter_b4_reservoir_80184F80;

extern GpAreaVariant D_shelter_b4_reservoir_80187350[12];

extern u8 D_shelter_b4_reservoir_801850C8[16];

extern u8 D_shelter_b4_reservoir_801850D8[16];

// shelter_b4_reservoir
extern WorldCoordRoomLighting D_shelter_b4_reservoir_801850E8[];

extern GpRoomObjRec D_shelter_b4_reservoir_801850F8[];

extern u8* D_shelter_b4_reservoir_80185118[];

extern GpViewCountRec D_shelter_b4_reservoir_80185120[];

extern GpWarpRec D_shelter_b4_reservoir_80185124[];

extern ViewCamera D_shelter_b4_reservoir_80185ADC[];

extern SpriteView D_shelter_b4_reservoir_80186730[];

extern WorldCollisionSurfaceProperties* D_shelter_b4_reservoir_80187480[];

void func_shelter_b4_reservoir_8017E88C(Task* task);

void func_shelter_b4_reservoir_801813F0(Task* task);

void func_shelter_b4_reservoir_8017FB84(Task* task);

void func_shelter_b4_reservoir_801803DC(Task* task);

void func_shelter_b4_reservoir_80180864(Task* task);

void func_shelter_b4_reservoir_80182B1C(Task* arg0);

void func_shelter_b4_reservoir_80183074(Task* task);

void func_shelter_b4_reservoir_80183CD4(Task* arg0);

#endif // INCLUDE_ROOMS_SHELTER_B4_RESERVOIR_H
