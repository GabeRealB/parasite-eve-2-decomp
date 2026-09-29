#ifndef ROOMS_DRYFIELD_WAREHOUSE_H
#define ROOMS_DRYFIELD_WAREHOUSE_H

#include "gameplay/area.h"

#include "common.h"

#include "main/task_types.h"

/// The descriptors of the warehouse's cutscene tasks, spawned by index: the
/// cutscene task itself, the screen fade that ramps its channels up from 0, and
/// the one that ramps them down from 0xFF.
extern TaskDesc D_dryfield_warehouse_8017FB08[];

void func_dryfield_warehouse_8017F494(Task* arg0);
extern GpAreaVariant D_dryfield_warehouse_80182100[13];

#endif // ROOMS_DRYFIELD_WAREHOUSE_H
