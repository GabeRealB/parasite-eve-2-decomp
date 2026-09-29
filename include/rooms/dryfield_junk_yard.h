#ifndef INCLUDE_ROOMS_DRYFIELD_JUNK_YARD_H
#define INCLUDE_ROOMS_DRYFIELD_JUNK_YARD_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern GpAreaVariant D_dryfield_junk_yard_8017F558[13];

/// Models the Dryfield map UI overlay's enemy descriptors attach.
extern TmdSource D_dryfield_junk_yard_8017ECE0;

// dryfield_junk_yard
extern GpRoomObjRec D_dryfield_junk_yard_8017ED04[];

extern GpRoomCoordRec D_dryfield_junk_yard_8017ED14[];

extern u8* D_dryfield_junk_yard_8017ED1C[];

extern GpViewCountRec D_dryfield_junk_yard_8017ED20[];

extern GpWarpRec D_dryfield_junk_yard_8017ED24[];

extern GpViewRec D_dryfield_junk_yard_8017F5C0[];

extern GpSprtRec D_dryfield_junk_yard_80180C28[];

extern GpRoomParamRec* D_dryfield_junk_yard_80181C28[];

void func_dryfield_junk_yard_8017DD0C(Task* unused);

void func_dryfield_junk_yard_8017D5F4(Task* task);

void func_dryfield_junk_yard_8017DCB4(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_JUNK_YARD_H
