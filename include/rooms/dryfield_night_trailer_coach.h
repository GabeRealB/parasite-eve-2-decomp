#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_TRAILER_COACH_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_TRAILER_COACH_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern TaskDesc D_dryfield_night_trailer_coach_801846D0;

extern GpAreaVariant D_dryfield_night_trailer_coach_8018C15C[13];

extern TmdSource gDryfieldNightTrailerCoachAcropolisSanctuaryModel090F0;

// dryfield_night_trailer_coach
extern WorldCoordRoomLighting D_dryfield_night_trailer_coach_80189500[];

extern GpRoomObjRec D_dryfield_night_trailer_coach_80189508[];

extern u8* D_dryfield_night_trailer_coach_80189518[];

extern GpViewCountRec D_dryfield_night_trailer_coach_8018951C[];

extern GpWarpRec D_dryfield_night_trailer_coach_80189520[];

extern ViewCamera D_dryfield_night_trailer_coach_80189A44[];

extern SpriteView D_dryfield_night_trailer_coach_8018B64C[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_trailer_coach_8018C1E8[];

void func_dryfield_night_trailer_coach_8018138C(Task* task);

void func_dryfield_night_trailer_coach_80182924(Task* unused);

void func_dryfield_night_trailer_coach_801828CC(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_TRAILER_COACH_H
