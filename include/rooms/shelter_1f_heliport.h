#ifndef ROOMS_SHELTER_1F_HELIPORT_H
#define ROOMS_SHELTER_1F_HELIPORT_H

#include "gameplay/area.h"

#include "main/task_types.h"

#include "common.h"

// Called by the actor overlay's event scripts while this room is loaded.
void func_shelter_1f_heliport_801802AC(s32 arg0);

extern TaskDesc D_shelter_1f_heliport_80181188;
extern GpAreaVariant D_shelter_1f_heliport_80182BF4[13];

void func_shelter_1f_heliport_80180B4C(Task* unused);

#endif // ROOMS_SHELTER_1F_HELIPORT_H
