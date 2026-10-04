#ifndef INCLUDE_ROOMS_SHELTER_B2_MAIN_CORRIDOR_H
#define INCLUDE_ROOMS_SHELTER_B2_MAIN_CORRIDOR_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// Room data exported to the shared waypoint actor.
extern SVECTOR* D_shelter_b2_main_corridor_80182EEC[4];

extern SVECTOR D_shelter_b2_main_corridor_80182EFC[12];

extern s16 D_shelter_b2_main_corridor_80182E28;

extern AreaVariant D_shelter_b2_main_corridor_8018933C[22];

// shelter_b2_main_corridor
extern u8* D_shelter_b2_main_corridor_801830CC[];

extern ViewCount D_shelter_b2_main_corridor_801830D0[];

extern DirectionWarpEntry D_shelter_b2_main_corridor_801830D4[];

extern WorldCollisionGrid D_shelter_b2_main_corridor_80184440;

extern ViewCamera D_shelter_b2_main_corridor_80184464[];

extern SpriteView D_shelter_b2_main_corridor_80188848[];

extern WorldCoordRoomLights D_shelter_b2_main_corridor_80188BE4;

extern WorldCollisionTrigger D_shelter_b2_main_corridor_80188BFC[];

extern WorldCollisionTrigger D_shelter_b2_main_corridor_801893EC[];

extern WorldCollisionSurfaceProperties* D_shelter_b2_main_corridor_80189624[];

void func_shelter_b2_main_corridor_8017E338(Task* task);

void func_shelter_b2_main_corridor_8017EF24(Task* task);

void waterDriftTaskNoUpdate(Task* task);

void func_shelter_b2_main_corridor_8018094C(Task* task);

void func_shelter_b2_main_corridor_801813B0(Task* task);

void func_shelter_b2_main_corridor_80181C98(Task* task);

void func_shelter_b2_main_corridor_8017EC34(Task* arg0);

#endif // INCLUDE_ROOMS_SHELTER_B2_MAIN_CORRIDOR_H
