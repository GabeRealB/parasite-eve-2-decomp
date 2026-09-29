#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_BACK_STREET_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_BACK_STREET_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_dryfield_night_back_street_80181518[22];

// dryfield_night_back_street
extern GpRoomObjRec D_dryfield_night_back_street_801803AC[];

extern GpRoomCoordRec D_dryfield_night_back_street_801803BC[];

extern u8* D_dryfield_night_back_street_801803C4[];

extern GpViewCountRec D_dryfield_night_back_street_801803C8[];

extern GpWarpRec D_dryfield_night_back_street_801803CC[];

extern GpViewRec D_dryfield_night_back_street_80180B58[];

extern GpSprtRec D_dryfield_night_back_street_80180D34[];

extern GpRoomParamRec* D_dryfield_night_back_street_8018161C[];

void func_dryfield_night_back_street_8017E390(Task* task);

void func_dryfield_night_back_street_8017EDF4(Task* task);

void func_dryfield_night_back_street_8017F6DC(Task* task);

void func_dryfield_night_back_street_8017D7E0(Task* arg0);

void func_dryfield_night_back_street_8017D788(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_BACK_STREET_H
