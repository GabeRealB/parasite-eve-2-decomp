#ifndef ROOMS_SHARED_8017F49C_H
#define ROOMS_SHARED_8017F49C_H

#include "common.h"

#include "gameplay/4CC.h"
#include "main/task.h"

void RoomsShared8017f49c(Task* task);

/// Per-room data of the charge panel: its "Charge" caption plus the two work
/// words holding the animating bar value and the slot map it is stepping.
extern u8         RoomsShared8017f49cCharge[];
extern s32        RoomsShared8017f49cQty;
extern GpItemMap* RoomsShared8017f49cMap;

#endif // ROOMS_SHARED_8017F49C_H
