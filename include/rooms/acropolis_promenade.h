#ifndef INCLUDE_ROOMS_ACROPOLIS_PROMENADE_H
#define INCLUDE_ROOMS_ACROPOLIS_PROMENADE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_acropolis_promenade_80183020[17];

// acropolis_promenade
extern GpRoomObjRec D_acropolis_promenade_80181B90[];

extern u8* D_acropolis_promenade_80181BC0[];

extern GpViewCountRec D_acropolis_promenade_80181BC8[];

extern GpRoomCoordRec D_acropolis_promenade_80181BCC[];

extern GpWarpRec D_acropolis_promenade_80181BDC[];

extern GpSprtRec D_acropolis_promenade_80185FB4[];

extern GpViewRec D_acropolis_promenade_80186050[];

extern GpRoomParamRec* D_acropolis_promenade_801862B0[];

void func_acropolis_promenade_8017E03C(Task* task);

void func_acropolis_promenade_8017E634(Task* task);

void func_acropolis_promenade_8017E394(Task* task);

void func_acropolis_promenade_8017ED44(Task* task);

void func_acropolis_promenade_8017F0BC(Task* task);

void func_acropolis_promenade_8017DA4C(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_PROMENADE_H
