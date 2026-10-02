#ifndef INCLUDE_ROOMS_DRYFIELD_BREEZEWAY_H
#define INCLUDE_ROOMS_DRYFIELD_BREEZEWAY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_dryfield_breezeway_801842F8[13];

// dryfield_breezeway
extern WorldCollisionRoomResources D_dryfield_breezeway_8018316C[];

extern u8* D_dryfield_breezeway_8018317C[];

extern WorldCoordRoomLighting D_dryfield_breezeway_80183180[];

extern ViewCount D_dryfield_breezeway_80183188[];

extern GpWarpRec D_dryfield_breezeway_8018318C[];

extern ViewCamera D_dryfield_breezeway_8018364C[];

extern SpriteView D_dryfield_breezeway_80183D9C[];

extern WorldCollisionSurfaceProperties* D_dryfield_breezeway_8018437C[];

void func_dryfield_breezeway_80181264(Task* task);

void func_dryfield_breezeway_8017FF7C(Task* task);

void func_dryfield_breezeway_8017DE68(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_BREEZEWAY_H
