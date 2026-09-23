#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/task.h"
#include "main/tmd.h"

typedef struct {
    u8             pad0[8];
    GsCOORDINATE2* field_8;
    u8             pad_C[0x16];
    u16            field_22;
} ClumpMem;

void func_shelter_r48_8017FF74(GsCOORDINATE2* arg0, s32 arg1, s32 arg2);
void func_shelter_r48_8017F124(GpEffWork* work, GsCOORDINATE2* coord, s32 part);
void func_shelter_r48_8018258C(void* arg0, s32 arg1, s32 arg2);
void func_shelter_r48_80180804(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);

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

void func_shelter_r48_8017E4C4(Task* arg0)
{
    ClumpMem*      mem;
    GsCOORDINATE2* coord;
    MATRIX*        m;
    s32            i;

    mem   = (ClumpMem*)arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        func_shelter_r48_8017FF74(coord, ((s16)mem->field_22 / 2) & 0xFFFF, 0x380);
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }
    if (arg0->state == 0) {
        m                            = &coord->coord;
        coord->sub                   = mem->field_8;
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
        arg0->state = 1;
    }
    mem->field_22 += 1;
    switch (arg0->spawnArg1) {
        case 0:
            Gp_SpawnEff(0x6018B, coord, 0x14002400, NULL);
            arg0->spawnArg1 = 1;
            return;
        case 1:
            func_shelter_r48_8017FF74(coord, ((s16)mem->field_22 / 2) & 0xFFFF, 0x380);
            if (!(mem->field_22 & 1)) {
                Gp_SpawnEff(0x6018B, coord, 0x1001400, NULL);
            }
            mem->field_22 += 1;
            return;
        case 2:
            Gp_SpawnEff(0x6018B, coord, 0x10002380, NULL);
            for (i = 0; i < 4; i++) {
                Gp_SpawnEff(0x6018B, coord, 0x2002400, NULL);
                Gp_SpawnEff(0x6018C, coord, 0x2202300, NULL);
            }
            arg0->spawnArg1 = 3;
            return;
        case 3:
            Gp_ReleaseState1CMem(mem, arg0);
            return;
    }
}

void func_shelter_r48_8017E704(Task* arg0)
{
    ClumpMem*      mem;
    GsCOORDINATE2* coord;
    MATRIX*        m;

    mem   = (ClumpMem*)arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        func_shelter_r48_80180804(coord, ((s16)((s16)mem->field_22 / 2) % 12) & 0xFFFF, 0x800, 0);
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }
    if (arg0->state == 0) {
        m                            = &coord->coord;
        coord->sub                   = mem->field_8;
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
        arg0->state = 1;
    }
    mem->field_22 += 1;
    switch (arg0->spawnArg1) {
        case 0:
            Gp_SpawnEff(0x6018B, coord, 0x14002800, NULL);
            arg0->spawnArg1 = 1;
            return;
        case 1:
            func_shelter_r48_80180804(coord, ((s16)((s16)mem->field_22 / 2) % 12) & 0xFFFF, 0x800, 0);
            if (!(mem->field_22 & 1)) {
                Gp_SpawnEff(0x6018B, coord, 0x12801800, NULL);
            }
            mem->field_22 += 1;
            return;
        case 2:
            if ((s16)((s16)mem->field_22 % 6) == 0) {
                Gp_SpawnEff(0x6018C, coord, 0x2802800, NULL);
            }
            if (!(mem->field_22 & 1)) {
                Gp_SpawnEff(0x6018B, coord, 0x12803800, NULL);
            }
            return;
    }
}

void func_shelter_r48_8017E9B8(Task* arg0)
{
    ClumpMem*      mem;
    GsCOORDINATE2* coord;
    MATRIX*        m;

    mem   = (ClumpMem*)arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        func_shelter_r48_80180804(coord, ((s16)((s16)mem->field_22 / 2) % 12 | 0x1000) & 0xFFFF, 0xA00, 0);
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }
    if (arg0->state == 0) {
        m                            = &coord->coord;
        coord->sub                   = mem->field_8;
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
        arg0->state = 1;
    }
    mem->field_22 += 1;
    switch (arg0->spawnArg1) {
        case 0:
            Gp_SpawnEff(0x6018C, coord, 0x94002A00, NULL);
            arg0->spawnArg1 = 1;
            return;
        case 1:
            func_shelter_r48_80180804(coord, ((s16)((s16)mem->field_22 / 2) % 12 | 0x1000) & 0xFFFF, 0x800, 0);
            if (!(mem->field_22 & 1)) {
                Gp_SpawnEff(0x6018C, coord, 0x92801800, NULL);
            }
            mem->field_22 += 1;
            return;
        case 2:
            if (!(mem->field_22 & 1)) {
                Gp_SpawnEff(0x6018C, coord, 0x92603C00, NULL);
            }
            return;
    }
}

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
