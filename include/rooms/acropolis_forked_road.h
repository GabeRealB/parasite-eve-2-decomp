#ifndef INCLUDE_ROOMS_ACROPOLIS_FORKED_ROAD_H
#define INCLUDE_ROOMS_ACROPOLIS_FORKED_ROAD_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_acropolis_forked_road_801831C4[24];

// acropolis_forked_road
extern WorldCollisionRoomResources D_acropolis_forked_road_80182214[];

extern u8* D_acropolis_forked_road_8018225C[];

extern ViewCount D_acropolis_forked_road_80182268[];

extern WorldCoordRoomLighting D_acropolis_forked_road_80182270[];

extern DirectionWarpEntry D_acropolis_forked_road_80182288[];

extern SpriteView D_acropolis_forked_road_801844E0[];

extern ViewCamera D_acropolis_forked_road_80184E88[];

extern WorldCollisionSurfaceProperties* D_acropolis_forked_road_801850A4[];

void func_acropolis_forked_road_8017E298(Task* task);

void func_acropolis_forked_road_8017E410(Task* task);

void func_acropolis_forked_road_8017EF80(Task* task);

void func_acropolis_forked_road_8017F9E4(Task* task);

void func_acropolis_forked_road_801802CC(Task* task);

void func_acropolis_forked_road_8017E81C(Task* task);

void func_acropolis_forked_road_8017D9CC(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_FORKED_ROAD_H
