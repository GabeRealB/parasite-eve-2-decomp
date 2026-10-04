#ifndef INCLUDE_ROOMS_NEO_ARK_PAVILION_H
#define INCLUDE_ROOMS_NEO_ARK_PAVILION_H

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
extern SVECTOR* D_neo_ark_pavilion_80183A64[4];

extern SVECTOR D_neo_ark_pavilion_80183A74[8];

extern s16 D_neo_ark_pavilion_801839A0;

extern TaskDesc D_neo_ark_pavilion_8018384C;

extern AreaVariant D_neo_ark_pavilion_801876C4[13];

// neo_ark_pavilion
extern WorldCollisionRoomResources D_neo_ark_pavilion_801838B4[];

extern WorldCoordRoomLighting D_neo_ark_pavilion_801838D4[];

extern u8* D_neo_ark_pavilion_801838EC[];

extern ViewCount D_neo_ark_pavilion_801838F4[];

extern DirectionWarpEntry D_neo_ark_pavilion_801838F8[];

extern ViewCamera D_neo_ark_pavilion_80184208[];

extern SpriteView D_neo_ark_pavilion_801873B8[];

extern WorldCollisionSurfaceProperties* D_neo_ark_pavilion_801879EC[];

void func_neo_ark_pavilion_8017FC10(Task* arg0);

void func_neo_ark_pavilion_8017EC4C(Task* task);

void func_neo_ark_pavilion_8017F0CC(Task* task);

void func_neo_ark_pavilion_8017FCB0(Task* task);

void func_neo_ark_pavilion_80180714(Task* task);

void func_neo_ark_pavilion_80180FFC(Task* task);

void func_neo_ark_pavilion_80181C44(Task* arg0);

void func_neo_ark_pavilion_8018219C(Task* task);

void func_neo_ark_pavilion_80182DFC(Task* arg0);

void func_neo_ark_pavilion_8017EBF4(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_PAVILION_H
