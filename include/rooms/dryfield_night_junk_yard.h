#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_JUNK_YARD_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_JUNK_YARD_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_dryfield_night_junk_yard_801843F0[22];

// dryfield_night_junk_yard
extern GpRoomCoordRec D_dryfield_night_junk_yard_80180784[];

extern GpRoomObjRec D_dryfield_night_junk_yard_80180794[];

extern u8* D_dryfield_night_junk_yard_801807C0[];

extern GpViewCountRec D_dryfield_night_junk_yard_801807C8[];

extern GpWarpRec D_dryfield_night_junk_yard_801807CC[];

extern GpViewRec D_dryfield_night_junk_yard_801811DC[];

extern GpSprtRec D_dryfield_night_junk_yard_80183700[];

extern GpRoomParamRec* D_dryfield_night_junk_yard_801844C4[];

void func_dryfield_night_junk_yard_8017D9B8(u8 arg0);

void func_dryfield_night_junk_yard_8017E5C8(Task* task);

void func_dryfield_night_junk_yard_8017F02C(Task* task);

void func_dryfield_night_junk_yard_8017F914(Task* task);

void func_dryfield_night_junk_yard_8017DA14(Task* task);

void func_dryfield_night_junk_yard_8017D960(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_JUNK_YARD_H
