#ifndef INCLUDE_ROOMS_DRYFIELD_DILAPIDATED_HOUSE_H
#define INCLUDE_ROOMS_DRYFIELD_DILAPIDATED_HOUSE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_dryfield_dilapidated_house_80189938[13];

// dryfield_dilapidated_house
extern GpRoomObjRec D_dryfield_dilapidated_house_80186954[];

extern u8* D_dryfield_dilapidated_house_80186964[];

extern ViewCount D_dryfield_dilapidated_house_80186968[];

extern WorldCoordRoomLighting D_dryfield_dilapidated_house_8018696C[];

extern GpWarpRec D_dryfield_dilapidated_house_80186974[];

extern ViewCamera D_dryfield_dilapidated_house_80187308[];

extern SpriteView D_dryfield_dilapidated_house_80188C0C[];

extern WorldCollisionSurfaceProperties* D_dryfield_dilapidated_house_80189A80[];

void func_dryfield_dilapidated_house_80183BF8(Task* arg0);

void func_dryfield_dilapidated_house_80181F08(Task* task);

void func_dryfield_dilapidated_house_80182744(Task* task);

void func_dryfield_dilapidated_house_80183C8C(Task* arg0);

void func_dryfield_dilapidated_house_80183D5C(Task* arg0);

void func_dryfield_dilapidated_house_8017EB60(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_DILAPIDATED_HOUSE_H
