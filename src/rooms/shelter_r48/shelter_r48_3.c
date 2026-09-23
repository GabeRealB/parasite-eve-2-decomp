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

void func_shelter_r48_8018147C(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    MATRIX*        m;
    u8             rgb[3];

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        func_shelter_r48_8017F124(work, coord, 3);
        func_shelter_r48_8017F124(work, coord, 4);
        func_shelter_r48_8017F124(work, coord, 5);
        if (Gp_State1C->eventState < 4) {
            return;
        }
    } else {
        work->age++;
        switch (task->state) {
            case 0:
                m                            = &coord->coord;
                coord->sub                   = work->parent;
                *(s32*)&coord->coord.m[0][0] = 0x1000;
                *(s32*)&m->m[0][2]           = 0;
                *(s32*)&m->m[1][1]           = 0x1000;
                *(s32*)&m->m[2][0]           = 0;
                m->m[2][2]                   = 0x1000;
                coord->coord.t[2]            = 0;
                coord->coord.t[1]            = 0;
                coord->coord.t[0]            = 0;
                coord->flg                   = 0;
                Gp_UpdateCoord(coord);
                work->age   = 1;
                work->scale = 0x80;
                task->state = 1;
                return;
            case 1:
                func_shelter_r48_8017F124(work, coord, 3);
                func_shelter_r48_8017F124(work, coord, 4);
                func_shelter_r48_8017F124(work, coord, 5);
                work->angle  += 0x10;
                work->period += 0x10;
                work->step   += 0x10;
                rgb[0]        = work->scale;
                rgb[1]        = (u16)work->scale >> 1;
                rgb[2]        = (u16)work->scale >> 2;
                Gp_DrawFadeQuad(rgb, 1);
                if (work->age >= 0x31) {
                    task->state = 2;
                }
                return;
            case 2:
                if (work->scale >= 0x11) {
                    work->scale  -= 0x10;
                    work->angle  += 0x10;
                    work->period += 0x10;
                    work->step   += 0x10;
                    func_shelter_r48_8017F124(work, coord, 3);
                    func_shelter_r48_8017F124(work, coord, 4);
                    func_shelter_r48_8017F124(work, coord, 5);
                    rgb[0] = work->scale;
                    rgb[1] = (u16)work->scale >> 1;
                    rgb[2] = (u16)work->scale >> 2;
                    Gp_DrawFadeQuad(rgb, 1);
                    return;
                }
                break;
            default:
                return;
        }
    }
    Gp_ReleaseState1CMem(work, task);
}

INCLUDE_ASM("rooms/nonmatchings/shelter_r48/shelter_r48_3", func_shelter_r48_80181704);

INCLUDE_ASM("rooms/nonmatchings/shelter_r48/shelter_r48_3", func_shelter_r48_80181C14);

INCLUDE_ASM("rooms/nonmatchings/shelter_r48/shelter_r48_3", func_shelter_r48_8018258C);
