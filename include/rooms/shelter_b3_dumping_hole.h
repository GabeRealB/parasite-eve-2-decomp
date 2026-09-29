#ifndef ROOMS_SHELTER_B3_DUMPING_HOLE_H
#define ROOMS_SHELTER_B3_DUMPING_HOLE_H

#include "gameplay/area.h"

#include "gameplay/world_coords.h"

#include "main/task_types.h"

#include "common.h"

s16  func_shelter_b3_dumping_hole_8017FB70(void);
void func_shelter_b3_dumping_hole_80183198(s16 arg0, s16 arg1, s16 arg2);

extern TaskDesc D_shelter_b3_dumping_hole_8018B83C[4];

// Retained task seed: keep the callback's actual two-argument ABI.
typedef struct {
    u16 flags;
    u16 priority;
    void (*callback)(Task*, s32);
    void* argument;
} DumpingHoleCaptionTaskSeed;
STATIC_ASSERT_SIZEOF(DumpingHoleCaptionTaskSeed, 12);
extern DumpingHoleCaptionTaskSeed D_shelter_b3_dumping_hole_8018B57C;

void func_shelter_b3_dumping_hole_8017FCF4(GpCoord* arg0, SVECTOR* arg1);

void func_shelter_b3_dumping_hole_80183F84(Task* task);
void func_shelter_b3_dumping_hole_8018521C(Task* task);
void func_shelter_b3_dumping_hole_80186218(Task* task);
void func_shelter_b3_dumping_hole_80186D4C(Task* arg0);
extern GpAreaVariant D_shelter_b3_dumping_hole_8018EC3C[13];

#endif // ROOMS_SHELTER_B3_DUMPING_HOLE_H
