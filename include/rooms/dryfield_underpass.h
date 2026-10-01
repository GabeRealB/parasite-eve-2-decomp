#ifndef INCLUDE_ROOMS_DRYFIELD_UNDERPASS_H
#define INCLUDE_ROOMS_DRYFIELD_UNDERPASS_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_dryfield_underpass_8017F868[13];

// dryfield_underpass
extern GpRoomObjRec D_dryfield_underpass_8017EB20[];

extern u8* D_dryfield_underpass_8017EBBC[];

extern GpViewCountRec D_dryfield_underpass_8017EBD4[];

extern GpRoomCoordRec D_dryfield_underpass_8017EBE0[];

extern GpWarpRec D_dryfield_underpass_8017EC10[];

extern GpViewRec D_dryfield_underpass_8017F4A8[];

extern GpSprtRec D_dryfield_underpass_80180250[];

extern WorldCollisionSurfaceProperties* D_dryfield_underpass_80181164[];

void func_dryfield_underpass_8017DE30(Task* task);

void func_dryfield_underpass_8017DAC8(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_UNDERPASS_H
