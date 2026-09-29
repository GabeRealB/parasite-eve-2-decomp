#ifndef ROOMS_DRYFIELD_NIGHT_GARAGE_H
#define ROOMS_DRYFIELD_NIGHT_GARAGE_H

#include "gameplay/area.h"

#include "types.h"

#include "common.h"

#include "main/task_types.h"

/// Returns the task of the room work object whose id is the current area and
/// stage with `arg0` in bits 12 and up, or NULL when there is none.
Task* func_dryfield_night_garage_80180A64(s32 arg0);

void func_dryfield_night_garage_80180414(s32 arg0);

extern TaskDesc D_dryfield_night_garage_80183380[2];

void func_dryfield_night_garage_801807E4(Task* arg0);
extern TaskDesc D_dryfield_night_garage_80181C2C;
extern GpAreaVariant D_dryfield_night_garage_801874BC[12];

void func_dryfield_night_garage_80181518(Task* unused);

#endif // ROOMS_DRYFIELD_NIGHT_GARAGE_H
