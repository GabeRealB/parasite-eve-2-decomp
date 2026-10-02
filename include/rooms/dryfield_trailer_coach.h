#ifndef INCLUDE_ROOMS_DRYFIELD_TRAILER_COACH_H
#define INCLUDE_ROOMS_DRYFIELD_TRAILER_COACH_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern TaskDesc D_dryfield_trailer_coach_80183F84;

extern GpAreaVariant D_dryfield_trailer_coach_801876F0[13];

extern TmdSource gDryfieldTrailerCoachAcropolisSanctuaryModel090F0;

// dryfield_trailer_coach
extern GpRoomObjRec D_dryfield_trailer_coach_801871CC[];

extern WorldCoordRoomLighting D_dryfield_trailer_coach_801871DC[];

extern u8* D_dryfield_trailer_coach_801871E4[];

extern ViewCount D_dryfield_trailer_coach_801871E8[];

extern GpWarpRec D_dryfield_trailer_coach_801871EC[];

extern ViewCamera D_dryfield_trailer_coach_80187758[];

extern SpriteView D_dryfield_trailer_coach_801891D0[];

extern WorldCollisionSurfaceProperties* D_dryfield_trailer_coach_80189C30[];

void func_dryfield_trailer_coach_801838DC(Task* arg0);

void func_dryfield_trailer_coach_80181364(Task* task);

void func_dryfield_trailer_coach_80182950(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_TRAILER_COACH_H
