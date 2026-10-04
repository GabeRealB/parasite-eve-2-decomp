#ifndef INCLUDE_ROOMS_DRYFIELD_MAIN_STREET_H
#define INCLUDE_ROOMS_DRYFIELD_MAIN_STREET_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_main_street_80184F20[13];

// dryfield_main_street
extern WorldCollisionRoomResources D_dryfield_main_street_80181BBC[];

extern u8* D_dryfield_main_street_80181BCC[];

extern ViewCount D_dryfield_main_street_80181BD0[];

extern WorldCoordRoomLighting D_dryfield_main_street_80181BD4[];

extern DirectionWarpEntry D_dryfield_main_street_80181BDC[];

extern ViewCamera D_dryfield_main_street_80182CC0[];

extern SpriteView D_dryfield_main_street_80184308[];

extern WorldCollisionSurfaceProperties* D_dryfield_main_street_801855EC[];

void dryfieldMainStreetPuffTask(Task* task);

void func_dryfield_main_street_8017EEE8(Task* task);

void func_dryfield_main_street_8017F94C(Task* task);

void func_dryfield_main_street_80180234(Task* task);

void func_dryfield_main_street_8017E4B0(Task* task);

void func_dryfield_main_street_8017E168(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_MAIN_STREET_H
