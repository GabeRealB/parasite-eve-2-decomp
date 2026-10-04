#ifndef INCLUDE_ROOMS_NEO_ARK_BRIDGE_H
#define INCLUDE_ROOMS_NEO_ARK_BRIDGE_H

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
extern SVECTOR* D_neo_ark_bridge_801820BC[4];

extern SVECTOR D_neo_ark_bridge_801820CC[9];

extern s16 D_neo_ark_bridge_80181FF8;

extern TaskDesc D_neo_ark_bridge_80181F18;

extern AreaVariant D_neo_ark_bridge_80184A50[13];

// neo_ark_bridge
extern u8* D_neo_ark_bridge_80181F80[];

extern ViewCount D_neo_ark_bridge_80181F84[];

extern DirectionWarpEntry D_neo_ark_bridge_80181F88[];

extern WorldCollisionGrid D_neo_ark_bridge_80182814;

extern ViewCamera D_neo_ark_bridge_80182838[];

extern SpriteView D_neo_ark_bridge_80184564[];

extern WorldCoordRoomLights D_neo_ark_bridge_8018470C;

extern WorldCollisionTrigger D_neo_ark_bridge_80184724[];

extern WorldCollisionTrigger D_neo_ark_bridge_80184AB8[];

extern WorldCollisionSurfaceProperties* D_neo_ark_bridge_80184BD4[];

void func_neo_ark_bridge_8017E954(Task* arg0);

void func_neo_ark_bridge_8017EF70(Task* task);

void func_neo_ark_bridge_8017F3F8(Task* task);

void func_neo_ark_bridge_8017FF84(Task* task);

void func_neo_ark_bridge_801809E8(Task* task);

void func_neo_ark_bridge_801812D0(Task* task);

void func_neo_ark_bridge_8017E8FC(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_BRIDGE_H
