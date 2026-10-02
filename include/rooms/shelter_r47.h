#ifndef INCLUDE_ROOMS_SHELTER_R47_H
#define INCLUDE_ROOMS_SHELTER_R47_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern TaskDesc D_shelter_r47_80187618;

extern WorldCollisionGrid D_shelter_r47_8018828C;

extern GpAreaApplyRec D_shelter_r47_8018A638[21];

extern GpAreaVariant D_shelter_r47_80187CB8[12];

// shelter_r47
extern u8* D_shelter_r47_80187674[];

extern ViewCount D_shelter_r47_80187678[];

extern DirectionWarpEntry D_shelter_r47_8018767C[];

extern WorldCollisionTrigger D_shelter_r47_801876B4[];

extern ViewCamera D_shelter_r47_801882B0[];

extern SpriteView D_shelter_r47_80189C68[];

extern WorldCoordRoomLights D_shelter_r47_8018A5BC;

extern WorldCollisionSurfaceProperties* D_shelter_r47_8018A618[];

extern WorldCollisionTrigger D_shelter_r47_8018787C[13];

void func_shelter_r47_801807B4(Task* task);

void func_shelter_r47_8017EC04(Task* task);

void func_shelter_r47_801858BC(Task* unused);

#endif // INCLUDE_ROOMS_SHELTER_R47_H
