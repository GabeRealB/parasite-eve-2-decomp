#ifndef ROOMS_SHELTER_B3_GARBAGE_INCINERATOR_H
#define ROOMS_SHELTER_B3_GARBAGE_INCINERATOR_H

#include "gameplay/area.h"

#include "gameplay/area_flags.h"

#include "main/task_types.h"

#include "types.h"

#include "common.h"

void func_shelter_b3_garbage_incinerator_8018108C(s16 arg0, s16 arg1, s16 arg2);
void func_shelter_b3_garbage_incinerator_801853C4(void);

extern u16 D_shelter_b3_garbage_incinerator_801855DE;

void func_shelter_b3_garbage_incinerator_80180FE4(s16 arg0, s16 arg1, s16 arg2);

void func_shelter_b3_garbage_incinerator_8018507C(void);

void func_shelter_b3_garbage_incinerator_80185220(void);

extern TaskDesc D_shelter_b3_garbage_incinerator_80187150[4];

extern u8 D_shelter_b3_garbage_incinerator_80187328[40];

extern GpAreaApplyRec D_shelter_b3_garbage_incinerator_8018FB6C[23];
extern u16 D_shelter_b3_garbage_incinerator_8018FBC8[2];

void func_shelter_b3_garbage_incinerator_8018110C(Task* task);
void func_shelter_b3_garbage_incinerator_80182368(Task* task);
void func_shelter_b3_garbage_incinerator_80183364(Task* task);
extern GpAreaVariant D_shelter_b3_garbage_incinerator_8018FA58[13];

#endif // ROOMS_SHELTER_B3_GARBAGE_INCINERATOR_H
