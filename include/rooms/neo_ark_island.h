#ifndef INCLUDE_ROOMS_NEO_ARK_ISLAND_H
#define INCLUDE_ROOMS_NEO_ARK_ISLAND_H

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
extern SVECTOR* D_neo_ark_island_80181CE8[4];

extern SVECTOR D_neo_ark_island_80181CF8[7];

extern s16 D_neo_ark_island_80181C24;

extern TaskDesc D_neo_ark_island_80181B30;

extern AreaVariant D_neo_ark_island_80183F48[13];

// neo_ark_island
extern WorldCollisionRoomResources D_neo_ark_island_80181B94[];

extern WorldCoordRoomLighting D_neo_ark_island_80181BA4[];

extern u8* D_neo_ark_island_80181BAC[];

extern ViewCount D_neo_ark_island_80181BB0[];

extern DirectionWarpEntry D_neo_ark_island_80181BB4[];

extern ViewCamera D_neo_ark_island_801826EC[];

extern SpriteView D_neo_ark_island_80183B14[];

extern WorldCollisionSurfaceProperties* D_neo_ark_island_80183FE8[];

void func_neo_ark_island_8017FB2C(Task* arg0);

void func_neo_ark_island_8017EB68(Task* task);

void waterDriftTaskU16FixedCoord(Task* task);

void func_neo_ark_island_8017FB9C(Task* task);

void func_neo_ark_island_80180600(Task* task);

void func_neo_ark_island_80180EE8(Task* task);

void func_neo_ark_island_8017EB10(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_ISLAND_H
