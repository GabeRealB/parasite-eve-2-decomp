#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgte.h>

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
/// `gpf 1`. The `inline_c.h` macro of that name assembles to a different word.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

typedef struct {
    u8             pad0[8];
    GsCOORDINATE2* field_8;
    u8             pad_C[0x16];
    u16            field_22;
} ClumpMem;

void func_shelter_r48_8017FB7C(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_shelter_r48_8017FF74(GsCOORDINATE2* arg0, s32 arg1, s32 arg2);
void func_shelter_r48_8017F124(GpEffWork* work, GsCOORDINATE2* coord, s32 part);
void func_shelter_r48_8018258C(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_r48_80180804(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_shelter_r48_80181C14(GsCOORDINATE2* coord, s32 arg1, s32 arg2, s32 arg3);

extern u32     Gp_LcgState;
extern SVECTOR D_shelter_r48_8018300C;
extern u8      D_shelter_r48_8018BE54[6][16];

void func_shelter_r48_8017E27C(u8 arg0)
{
    GpAreaKey* loc = &gGameSession->at4.loc;
    GpSprtRec* rec = Gp_SprtTables[loc->stage - 1]->field_0[loc->area - 1];
    GpSprtCmd* cmd;

    if (arg0 == 0) {
        cmd            = rec[1].field_4;
        cmd[1].field_4 = 1;
        cmd[3].field_4 = 1;
        cmd            = rec[2].field_4;
        cmd[1].field_4 = 1;
        cmd[4].field_4 = 1;
        cmd            = rec[3].field_4;
        cmd[1].field_4 = 1;
        cmd[4].field_4 = 1;
        cmd            = rec[5].field_4;
        cmd[2].field_4 = 1;
        cmd[3].field_4 = 1;
        cmd            = rec[6].field_4;
        cmd[1].field_4 = 1;
        cmd[4].field_4 = 1;
        cmd            = rec[7].field_4;
        cmd[1].field_4 = 1;
        cmd[5].field_4 = 1;
        cmd            = rec[17].field_4;
        cmd[1].field_4 = 1;
        cmd[5].field_4 = 1;
    } else if (arg0 == 1) {
        cmd            = rec[1].field_4;
        cmd[1].field_4 = 0;
        cmd[3].field_4 = 0;
        cmd            = rec[2].field_4;
        cmd[1].field_4 = 0;
        cmd[4].field_4 = 0;
        cmd            = rec[3].field_4;
        cmd[1].field_4 = 0;
        cmd[4].field_4 = 0;
        cmd            = rec[5].field_4;
        cmd[2].field_4 = 0;
        cmd[3].field_4 = 0;
        cmd            = rec[6].field_4;
        cmd[1].field_4 = 0;
        cmd[4].field_4 = 0;
        cmd            = rec[7].field_4;
        cmd[1].field_4 = 0;
        cmd[5].field_4 = 0;
        cmd            = rec[17].field_4;
        cmd[1].field_4 = 0;
        cmd[5].field_4 = 0;
    }
}

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
            func_shelter_r48_8018258C(&D_shelter_r48_8018300C, 0x100, 0x5C40);
        } else if (GameFlag_GetNibble(0x100) == 2) {
            func_shelter_r48_8018258C(&D_shelter_r48_8018300C, 0x100, 0x504C);
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

void func_shelter_r48_8017EC18(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    GpMtxWords*    rot;
    u8             rgb[3];

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState < 4) {
        work->age++;
        switch (task->state) {
            case 0:
                rot               = (GpMtxWords*)&coord->coord;
                coord->sub        = work->parent;
                rot->w0           = 0x1000;
                rot->w1           = 0;
                rot->w2           = 0x1000;
                rot->w3           = 0;
                rot->h4           = 0x1000;
                coord->coord.t[2] = 0;
                coord->coord.t[1] = 0;
                coord->coord.t[0] = 0;
                coord->flg        = 0;
                Gp_UpdateCoord(coord);
                task->spawnArg1 = 0x5A;
                task->state     = 1;
                work->scale     = 0;
                work->angle     = 0x100;
                work->step      = 0x100 / task->spawnArg1;
            case 1:
                if (Gp_State1C->eventState != 0) {
                    rgb[0] = work->scale;
                    rgb[1] = work->scale;
                    rgb[2] = (u16)work->scale >> 2;
                    Gp_DrawRing(coord, work->angle, rgb);
                    Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                    if (work->scale >= 0x61) {
                        rgb[0] = work->period;
                        rgb[1] = work->period;
                        rgb[2] = (u16)work->period >> 2;
                        Gp_DrawArc(coord, (s16)(task->spawnArg1 << 8), 0x200, rgb);
                    }
                    return;
                }
                work->scale += work->step;
                work->angle += 0x18;
                task->spawnArg1--;
                rgb[0] = work->scale;
                rgb[1] = work->scale;
                rgb[2] = (u16)work->scale >> 2;
                Gp_DrawRing(coord, work->angle, rgb);
                Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                if (work->scale >= 0x61) {
                    work->period += (u16)work->step * 2;
                    rgb[0]        = work->period;
                    rgb[1]        = work->period;
                    rgb[2]        = (u16)work->period >> 2;
                    Gp_DrawArc(coord, (s16)(task->spawnArg1 << 8), 0x200, rgb);
                }
                if (task->spawnArg1 == 0) {
                    work->scale = 0xFF;
                    task->state = 2;
                    Gp_SpawnEff(0x6018F, coord, 0, NULL);
                }
                return;
            case 2:
                if (work->scale >= 0x11) {
                    rgb[0] = work->scale;
                    rgb[1] = work->scale;
                    rgb[2] = (u16)work->scale >> 2;
                    Gp_DrawRing(coord, work->angle, rgb);
                    Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                    if (Gp_State1C->eventState == 0) {
                        work->scale -= 0x10;
                        work->angle -= 0x60;
                    }
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

/// Per-frame update of an effect task drawn with `func_shelter_r48_8017FB7C`
/// (state 1) or `func_shelter_r48_8017FF74` (state 2). State 0 seeds the work
/// from `spawnArg1`: the two draw parameters (the second one random), the frame
/// period and, when the spawner left `field_10` zero, a velocity chosen by bits
/// 24-27 of `spawnArg1`, normalised and scaled by `field_2A` through the GTE. Later ticks draw, drift
/// the coordinate by that velocity with `vy` growing by 6 each tick, and advance
/// the frame every `field_28` ticks, releasing the task after frame 7. While an
/// event is running the task only draws, and it is released once the event
/// state reaches 4.
void func_shelter_r48_8017F6C0(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    SVECTOR*       vec;
    s32            kind;
    s32            step;
    s32            state;
    s32            level;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            if (task->state < 2) {
                func_shelter_r48_8017FB7C(coord, work->field_20, (s16)work->field_24, (s16)work->field_26);
            } else {
                func_shelter_r48_8017FF74(coord, work->field_20, (s16)work->field_24);
            }
            return;
        }
        Gp_ReleaseState1CMem(work, task);
        return;
    }
    Gp_UpdateCoord(coord);
    work->field_22++;
    switch (task->state) {
        case 0:
            work->field_24 = ((GpEffSpawnArg*)&task->spawnArg1)->field_0 & 0xFFF;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_26 = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (task->spawnArg1 & 0xF000) {
                step = (task->spawnArg1 >> 12) & 0xF;
            } else {
                step = 1;
            }
            work->field_28 = step;
            work->field_22 = 0;
            state          = 1;
            if (task->spawnArg1 & 0xF0000000) {
                state = 2;
            }
            task->state = state;
            if (((u16)work->field_10.vx | (u16)work->field_10.vy | (u16)work->field_10.vz) == 0) {
                if (task->spawnArg1 & 0xFF0000) {
                    level = (task->spawnArg1 >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->field_2A = level;
                kind           = ((GpEffSpawnArgHi*)&task->spawnArg1)->field_3;
                switch (kind & 0xF) {
                    case 0:
                        work->field_2A = 0;
                        break;
                    case 1:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = 0xFFC0 - (((u32)Gp_LcgState >> 16) & 0x7F);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 2:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 3:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        break;
                    case 5:
                        work->field_10.vx = work->field_18;
                        work->field_10.vy = work->field_1A;
                        work->field_10.vz = work->field_1C;
                        break;
                }
                vec = &work->field_10;
                VectorNormalSS(vec, vec);
                gte_lddp(work->field_2A);
                gte_ldsv(vec);
                gte_gpf12_real();
                gte_stsv(vec);
            } else {
                work->field_2A = 0x40;
            }
            return;
        case 1:
            func_shelter_r48_8017FB7C(coord, work->field_20, (s16)work->field_24, (s16)work->field_26);
            break;
        case 2:
            func_shelter_r48_8017FF74(coord, work->field_20, (s16)work->field_24);
            break;
        default:
            return;
    }
    if ((s16)work->field_2A != 0) {
        coord->coord.t[0] += work->field_10.vx;
        coord->coord.t[1] += work->field_10.vy;
        coord->coord.t[2] += work->field_10.vz;
        coord->flg         = 0;
        work->field_10.vy += 6;
    }
    if (((s16)work->field_22 % (s16)work->field_28) == 0) {
        work->field_20++;
        if ((s16)work->field_20 >= 8) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_r48/shelter_r48_3", func_shelter_r48_8017FB7C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r48/shelter_r48_3", func_shelter_r48_8017FF74);

INCLUDE_ASM("rooms/nonmatchings/shelter_r48/shelter_r48_3", func_shelter_r48_80180210);

INCLUDE_ASM("rooms/nonmatchings/shelter_r48/shelter_r48_3", func_shelter_r48_80180804);

INCLUDE_ASM("rooms/nonmatchings/shelter_r48/shelter_r48_3", func_shelter_r48_80180C5C);

void func_shelter_r48_801810B0(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    GpEffWork*     eff;
    u8             rgb[3];
    s32            step;
    s16            scale;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState < 4) {
        work->age++;
        switch (task->state) {
            case 0:
                coord->sub = work->parent;
                Gfx_RotMatrixX(&coord->coord, -0x400, 1);
                coord->coord.t[2] = 0;
                coord->coord.t[1] = 0;
                coord->coord.t[0] = 0;
                coord->flg        = 0;
                Gp_UpdateCoord(coord);
                task->state = 1;
                work->scale = 0;
                work->angle = 0x100;
                work->step  = 0x100 / task->spawnArg1;
            case 1:
                if (Gp_State1C->eventState != 0) {
                    rgb[0] = work->scale;
                    rgb[1] = (u16)work->scale >> 1;
                    rgb[2] = (u16)work->scale >> 2;
                    Gp_DrawRing(coord, work->angle, rgb);
                    Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                    Gp_DrawArc(coord, (s16)((task->spawnArg1 % 10) * (work->scale << 2)), 0x100, rgb);
                    return;
                }
                scale        = work->scale;
                step         = (u16)work->step;
                work->scale  = scale + step;
                work->angle += (u16)work->step * 8;
                task->spawnArg1--;
                rgb[0] = work->scale;
                rgb[1] = (u16)work->scale >> 1;
                rgb[2] = (u16)work->scale >> 2;
                Gp_DrawRing(coord, work->angle, rgb);
                Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                Gp_DrawArc(coord, (s16)((task->spawnArg1 % 10) * (work->scale << 2)), 0x100, rgb);
                if (task->spawnArg1 == 0) {
                    work->scale = 0xFF;
                    task->state = 2;
                    eff         = Gp_SpawnEff(0x60191, coord, 0, NULL);
                    if (eff != NULL) {
                        Task_Reparent(task, eff->task);
                    }
                }
                return;
            case 2:
                if (work->scale >= 5) {
                    rgb[0] = work->scale;
                    rgb[1] = (u16)work->scale >> 1;
                    rgb[2] = (u16)work->scale >> 2;
                    Gp_DrawRing(coord, work->angle, rgb);
                    Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                    if (Gp_State1C->eventState == 0) {
                        work->scale -= 4;
                    }
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

void func_shelter_r48_80181704(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    MATRIX*        m;
    u8             rgb[3];
    s32            step;
    s16            scale;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState < 4) {
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
                task->state     = 1;
                task->spawnArg1 = 0x1E;
                work->scale     = 0;
                work->angle     = 0x100;
                work->step      = 0x100 / task->spawnArg1;
            case 1:
                if (Gp_State1C->eventState != 0) {
                    rgb[0] = work->scale;
                    rgb[1] = (u16)work->scale >> 2;
                    rgb[2] = (u16)work->scale >> 1;
                    Gp_DrawRing(coord, work->angle, rgb);
                    Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                    Gp_DrawArc(coord, (s16)((u16)task->spawnArg1 * 16 + 0x800), 0x100, rgb);
                    rgb[0] >>= 1;
                    rgb[1] >>= 1;
                    rgb[2] >>= 1;
                    Gp_DrawArc(coord, (s16)((u16)task->spawnArg1 * 32 + 0xC00), 0xC0, rgb);
                    return;
                }
                scale        = work->scale;
                step         = (u16)work->step;
                work->scale  = scale + step;
                work->angle += (u16)work->step * 8;
                task->spawnArg1--;
                rgb[0] = work->scale;
                rgb[1] = (u16)work->scale >> 2;
                rgb[2] = (u16)work->scale >> 1;
                Gp_DrawRing(coord, work->angle, rgb);
                Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                Gp_DrawArc(coord, (s16)((u16)task->spawnArg1 * 16 + 0x800), 0x100, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                Gp_DrawArc(coord, (s16)((u16)task->spawnArg1 * 32 + 0xC00), 0xC0, rgb);
                if (task->spawnArg1 == 0) {
                    work->scale  = 0xFF;
                    task->state  = 2;
                    work->period = 0x600;
                    work->step   = 0;
                }
                return;
            case 2:
                if (work->scale >= 0x11) {
                    rgb[0] = work->scale;
                    rgb[1] = (u16)work->scale >> 2;
                    rgb[2] = (u16)work->scale >> 1;
                    Gp_DrawRing(coord, work->angle, rgb);
                    Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                    if (Gp_State1C->eventState == 0) {
                        work->scale -= 0x10;
                        work->angle -= 0x60;
                    }
                    Gp_DrawFadeQuad(rgb, 1);
                } else {
                    task->state = 3;
                }
                func_shelter_r48_80181C14(coord, work->period, work->step, 0xC36);
                func_shelter_r48_80181C14(coord, work->period, (s16)-work->step, 0xC36);
                return;
            case 3:
                func_shelter_r48_80181C14(coord, work->period, work->step, 0xC36);
                func_shelter_r48_80181C14(coord, work->period, (s16)-work->step, 0xC36);
                if (Gp_State1C->eventState == 0) {
                    if (work->step < 0x200) {
                        work->period -= 0x18;
                        work->step   += 0x10;
                    } else {
                        task->state = 4;
                    }
                }
                return;
            case 4:
                if (work->period > 0) {
                    func_shelter_r48_80181C14(coord, work->period, work->step, 0xC36);
                    func_shelter_r48_80181C14(coord, work->period, (s16)-work->step, 0xC36);
                    if (Gp_State1C->eventState == 0) {
                        work->period -= 0x30;
                    }
                    return;
                }
                break;
            default:
                return;
        }
    }
    Gp_ReleaseState1CMem(work, task);
}

INCLUDE_ASM("rooms/nonmatchings/shelter_r48/shelter_r48_3", func_shelter_r48_80181C14);

/// Projects `arg0` through `Gfx_ViewWorldMtx` and, when the GTE flag is
/// non-negative, queues a glow of gouraud `POLY_G4` wedges around the projected
/// point: an outer ring pairing a half-brightness wedge at full radius with a
/// full-brightness one at half radius, then four inner cross wedges in the
/// half-brightness colour. The depth is pulled 0xC0 towards the camera (clamped
/// at 16) before it sizes the radii and sorts the primitives. `arg1` is the
/// signed half-extent, and `arg2` packs `[flicker shift][r][g][b]` nibbles,
/// with `gDisplayState.animFrame & 1` shifted by the top nibble added to every
/// channel.
void func_shelter_r48_8018258C(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    RoomDraw05Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s32                t2;
    s32                ua;
    s32                ub;
    s32                uc;
    s32                frame;
    s32                packed;
    s32                blend;
    s32                r;
    s32                g;
    s32                b;
    s32                outer;
    s32                inner;
    s32                hr;
    s32                hg;
    s32                hb;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        tmp     = (*scratch = (u8*)*scratch - 0x14);
        block   = (RoomDraw05Scratch*)tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps_real();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz -= 0xC0;
        if (block->otz < 0x10) {
            block->otz = 0x10;
        }
        arg1        <<= 16;
        arg1        >>= 16;
        outer         = (arg1 * 64) / block->otz;
        frame         = gDisplayState.animFrame;
        block->rOuter = outer;
        inner         = (arg1 * 8) / block->otz;
        ang           = 0;
        packed        = arg2 << 16;
        blend         = (frame & 1) << (packed >> 28);
        r             = blend + ((packed >> 20) & 0xF0);
        g             = blend + ((packed >> 16) & 0xF0);
        b             = blend + ((arg2 & 0xF) << 4);
        block->rInner = inner;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            hr = (u8)r >> 1;
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            hg = (u8)g >> 1;
            hb = (u8)b >> 1;
            setRGB2(prim, hr, hg, hb);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        ang = 0x200;
        r   = (u8)hr;
        g   = (u8)hg;
        b   = (u8)hb;
        do {
            ua             = ang - 0x400;
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ua)) >> 13);
            prim->y0 = block->sy + ((block->rInner * rcos(ua)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            ub       = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(ub)) >> 13);
            prim->y3 = block->sy + ((block->rInner * rcos(ub)) >> 13);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(ub)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(ub)) >> 11);
            uc       = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(uc)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(uc)) >> 12);
            ang      = uc;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
}
