#ifndef INCLUDE_ROOMS_NEO_ARK_SAVANNA_ZONE_H
#define INCLUDE_ROOMS_NEO_ARK_SAVANNA_ZONE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_neo_ark_savanna_zone_80180864[13];

// neo_ark_savanna_zone
extern WorldCollisionRoomResources D_neo_ark_savanna_zone_8017F9E4[];

extern WorldCoordRoomLighting D_neo_ark_savanna_zone_8017F9F4[];

extern u8* D_neo_ark_savanna_zone_8017F9FC[];

extern ViewCount D_neo_ark_savanna_zone_8017FA00[];

extern DirectionWarpEntry D_neo_ark_savanna_zone_8017FA04[];

extern ViewCamera D_neo_ark_savanna_zone_8017FBF4[];

extern SpriteView D_neo_ark_savanna_zone_801803F4[];

extern WorldCollisionSurfaceProperties* D_neo_ark_savanna_zone_80180968[];

void func_neo_ark_savanna_zone_8017D9AC(Task* arg0);

void func_neo_ark_savanna_zone_8017DA0C(Task* task);

void func_neo_ark_savanna_zone_8017E470(Task* task);

void func_neo_ark_savanna_zone_8017ED58(Task* task);

void func_neo_ark_savanna_zone_8017D954(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_SAVANNA_ZONE_H
