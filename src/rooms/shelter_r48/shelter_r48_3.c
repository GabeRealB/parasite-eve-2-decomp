#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/task.h"
#include "main/tmd.h"

void func_shelter_r48_8017F124(GpEffWork* work, GsCOORDINATE2* coord, s32 part);
void func_shelter_r48_8018258C(void* arg0, s32 arg1, s32 arg2);

extern u32 Gp_LcgState;
extern u8  D_shelter_r48_8018300C[];
extern u8  D_shelter_r48_8018BE54[6][16];

INCLUDE_ASM("rooms/nonmatchings/shelter_r48/shelter_r48_3", func_shelter_r48_8017E27C);

void func_shelter_r48_8017E3B8(Task* task)
{
    s32 viewMask;
    s32 i;
    s32 j;

    viewMask = 1 << Gp_GetViewIndex();
    if (task->state == 0) {
        Gp_State1C->groundTrace = 0;
        for (i = 0; i < 6; i++) {
            for (j = 0; j < 16; j++) {
                D_shelter_r48_8018BE54[i][j] = (Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16;
            }
        }
        task->state = 1;
    }
    if (viewMask & 0x401D8) {
        if (GameFlag_GetNibble(0x100) == 1) {
            func_shelter_r48_8018258C(D_shelter_r48_8018300C, 0x100, 0x5C40);
        } else if (GameFlag_GetNibble(0x100) == 2) {
            func_shelter_r48_8018258C(D_shelter_r48_8018300C, 0x100, 0x504C);
        }
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_r48/shelter_r48_3", func_shelter_r48_8017E4C4);

INCLUDE_ASM("rooms/nonmatchings/shelter_r48/shelter_r48_3", func_shelter_r48_8017E704);

INCLUDE_ASM("rooms/nonmatchings/shelter_r48/shelter_r48_3", func_shelter_r48_8017E9B8);

INCLUDE_ASM("rooms/nonmatchings/shelter_r48/shelter_r48_3", func_shelter_r48_8017EC18);

void func_shelter_r48_8017EFD8(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        func_shelter_r48_8017F124(work, coord, 0);
        func_shelter_r48_8017F124(work, coord, 1);
        func_shelter_r48_8017F124(work, coord, 2);
        if (Gp_State1C->eventState < 4) {
            return;
        }
    } else {
        work->age++;
        if (task->state == 0) {
            work->age   = 1;
            work->scale = 0x80;
            task->state = 1;
        }
        if (work->scale >= 9) {
            work->scale -= 8;
            work->angle += 0x200;
            work->step  += 0x100;
            func_shelter_r48_8017F124(work, coord, 0);
            func_shelter_r48_8017F124(work, coord, 1);
            func_shelter_r48_8017F124(work, coord, 2);
            return;
        }
    }
    Gp_ReleaseState1CMem(work, task);
}

INCLUDE_ASM("rooms/nonmatchings/shelter_r48/shelter_r48_3", func_shelter_r48_8017F124);

INCLUDE_ASM("rooms/nonmatchings/shelter_r48/shelter_r48_3", func_shelter_r48_8017F6C0);

INCLUDE_ASM("rooms/nonmatchings/shelter_r48/shelter_r48_3", func_shelter_r48_8017FB7C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r48/shelter_r48_3", func_shelter_r48_8017FF74);

INCLUDE_ASM("rooms/nonmatchings/shelter_r48/shelter_r48_3", func_shelter_r48_80180210);

INCLUDE_ASM("rooms/nonmatchings/shelter_r48/shelter_r48_3", func_shelter_r48_80180804);

INCLUDE_ASM("rooms/nonmatchings/shelter_r48/shelter_r48_3", func_shelter_r48_80180C5C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r48/shelter_r48_3", func_shelter_r48_801810B0);

INCLUDE_ASM("rooms/nonmatchings/shelter_r48/shelter_r48_3", func_shelter_r48_8018147C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r48/shelter_r48_3", func_shelter_r48_80181704);

INCLUDE_ASM("rooms/nonmatchings/shelter_r48/shelter_r48_3", func_shelter_r48_80181C14);

INCLUDE_ASM("rooms/nonmatchings/shelter_r48/shelter_r48_3", func_shelter_r48_8018258C);
