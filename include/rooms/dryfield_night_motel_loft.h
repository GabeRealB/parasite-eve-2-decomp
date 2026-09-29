#ifndef ROOMS_DRYFIELD_NIGHT_MOTEL_LOFT_H
#define ROOMS_DRYFIELD_NIGHT_MOTEL_LOFT_H

#include "gameplay/area.h"

#include "common.h"

/// Resets the room's live grid from its template and, when `arg0` is set,
/// raises the grid's four corners by 0xBB8 in Y.
void func_dryfield_night_motel_loft_8017D9BC(s32 arg0);

#include "main/task_types.h"

void func_dryfield_night_motel_loft_8017E090(Task* task);

void func_dryfield_night_motel_loft_8017DB64(Task* arg0);
extern GpAreaVariant D_dryfield_night_motel_loft_80180888[13];

#endif // ROOMS_DRYFIELD_NIGHT_MOTEL_LOFT_H
