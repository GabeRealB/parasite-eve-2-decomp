#ifndef ROOMS_SHELTER_B2_LABORATORY_H
#define ROOMS_SHELTER_B2_LABORATORY_H

#include "gameplay/area.h"

#include "main/task_types.h"

#include "gameplay/area_flags.h"

#include "common.h"

// Called by the actor overlay's event scripts while this room is loaded.
void func_shelter_b2_laboratory_801804FC(void);

extern GpAreaApplyRec D_shelter_b2_laboratory_80186488[5];

extern GpAreaApplyRec D_shelter_b2_laboratory_8018649C[2];

void func_shelter_b2_laboratory_80180548(Task* task);
extern GpAreaVariant D_shelter_b2_laboratory_80186360[11];

void func_shelter_b2_laboratory_8017EAB4(Task* task);

#endif // ROOMS_SHELTER_B2_LABORATORY_H
