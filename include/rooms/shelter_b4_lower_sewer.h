#ifndef INCLUDE_ROOMS_SHELTER_B4_LOWER_SEWER_H
#define INCLUDE_ROOMS_SHELTER_B4_LOWER_SEWER_H

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
extern SVECTOR* D_shelter_b4_lower_sewer_8018210C[4];

extern SVECTOR D_shelter_b4_lower_sewer_8018211C[10];

extern s16 D_shelter_b4_lower_sewer_80181E6C;

extern GpAreaVariant D_shelter_b4_lower_sewer_80183D48[12];

// shelter_b4_lower_sewer
extern u8* D_shelter_b4_lower_sewer_80181FA4[];

extern ViewCount D_shelter_b4_lower_sewer_80181FA8[];

extern DirectionWarpEntry D_shelter_b4_lower_sewer_80181FAC[];

extern WorldCollisionGrid D_shelter_b4_lower_sewer_801828E4;

extern ViewCamera D_shelter_b4_lower_sewer_80182908[];

extern SpriteView D_shelter_b4_lower_sewer_80182E80[];

extern WorldCoordRoomLights D_shelter_b4_lower_sewer_8018342C;

extern WorldCollisionTrigger D_shelter_b4_lower_sewer_80183444[];

extern WorldCollisionTrigger D_shelter_b4_lower_sewer_801837D4[];

extern WorldCollisionSurfaceProperties* D_shelter_b4_lower_sewer_80183DF4[];

void func_shelter_b4_lower_sewer_8017D6D4(Task* task);

void func_shelter_b4_lower_sewer_8017FEB0(Task* task);

void func_shelter_b4_lower_sewer_80180914(Task* task);

void func_shelter_b4_lower_sewer_801811FC(Task* task);

void func_shelter_b4_lower_sewer_8017EEE4(Task* task);

void func_shelter_b4_lower_sewer_8017F36C(Task* task);

void func_shelter_b4_lower_sewer_8017E400(Task* arg0);

#endif // INCLUDE_ROOMS_SHELTER_B4_LOWER_SEWER_H
