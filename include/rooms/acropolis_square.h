#ifndef ROOMS_ACROPOLIS_SQUARE_H
#define ROOMS_ACROPOLIS_SQUARE_H

#include "gameplay/area.h"

#include "main/task_types.h"

#include "types.h"

#include "gameplay/area_flags.h"

// The copied room-event handler uses this table only in stage 1, area 1.
extern GpAreaApplyRec D_acropolis_square_80188888[4];

s32 func_acropolis_square_80182360(s32 unused);

void func_acropolis_square_801823DC(Task* task);
void func_acropolis_square_801825DC(Task* task);
extern GpAreaVariant D_acropolis_square_80185E50[3];

void func_acropolis_square_80180804(Task* task);

void func_acropolis_square_8017F41C(Task* task);

#endif // ROOMS_ACROPOLIS_SQUARE_H
