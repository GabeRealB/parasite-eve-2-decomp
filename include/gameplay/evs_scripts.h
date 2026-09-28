#ifndef GAMEPLAY_EVS_SCRIPTS_H
#define GAMEPLAY_EVS_SCRIPTS_H

#include "types.h"

#include "gameplay/evs.h"

#include "main/task.h"

extern u8 D_801156F9;

Task* Gp_LookupSlot4(s32 arg0);

void func_800E8614(s32 arg0, s32 arg1);

void func_800E8634(GpEvsAddress arg0, s32 arg1, GpEvsAddress arg2);

void Gp_ShakeTask(Task* arg0);

void Gp_SndFadeTask(Task* arg0);

void Gp_VolFadeTask(Task* arg0);

void func_800E8830(Task* arg0);

void func_800E8888(Task* arg0);

#endif // GAMEPLAY_EVS_SCRIPTS_H
