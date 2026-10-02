#ifndef INCLUDE_ROOMS_SHELTER_B1_GOLEM_FREEZER_1_H
#define INCLUDE_ROOMS_SHELTER_B1_GOLEM_FREEZER_1_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_shelter_b1_golem_freezer_1_8017F17C[23];

// shelter_b1_golem_freezer_1
extern u8* D_shelter_b1_golem_freezer_1_8017E790[];

extern ViewCount D_shelter_b1_golem_freezer_1_8017E794[];

extern DirectionWarpEntry D_shelter_b1_golem_freezer_1_8017E798[];

extern WorldCollisionGrid D_shelter_b1_golem_freezer_1_8017E9C0;

extern ViewCamera D_shelter_b1_golem_freezer_1_8017E9E4[];

extern SpriteView D_shelter_b1_golem_freezer_1_8017EDB0[];

extern WorldCoordRoomLights D_shelter_b1_golem_freezer_1_8017EE64;

extern WorldCollisionTrigger D_shelter_b1_golem_freezer_1_8017EE7C[];

extern WorldCollisionTrigger D_shelter_b1_golem_freezer_1_8017EFAC[];

extern WorldCoordRoomAmbientEntry D_shelter_b1_golem_freezer_1_8017F234[];

extern WorldCollisionSurfaceProperties* D_shelter_b1_golem_freezer_1_8017F290[];

void func_shelter_b1_golem_freezer_1_8017D6EC(Task* task);

void func_shelter_b1_golem_freezer_1_8017DFFC(Task* task);

void func_shelter_b1_golem_freezer_1_8017DA7C(Task* unused);

#endif // INCLUDE_ROOMS_SHELTER_B1_GOLEM_FREEZER_1_H
