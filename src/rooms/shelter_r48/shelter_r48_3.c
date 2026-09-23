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
/// `rtv0`. The `inline_c.h` macro of that name assembles to a different word.
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")

typedef struct {
    u8             pad0[8];
    GsCOORDINATE2* field_8;
    u8             pad_C[0x16];
    u16            field_22;
} ClumpMem;

/// 0x2C-byte scratch block `func_shelter_r48_80181C14` takes from
/// `G_SCRATCH_HEAD`: the coordinate's world position, the tip point offset from
/// it, and both points' projections. `otz0`/`sx0`/`sy0` belong to `base`,
/// `otz1`/`sx1`/`sy1` to `tip`; `r0`/`r1` are the wedge radii at each end.
typedef struct {
    SVECTOR base;
    SVECTOR tip;
    s32     otz0;
    s32     otz1;
    s32     flag;
    s32     r0;
    s32     r1;
    u16     sx0;
    u16     sy0;
    u16     sx1;
    u16     sy1;
} _ShelterR48BeamScratch;

/// Per-band radius and height offsets `func_shelter_r48_8017F124` adds to the
/// effect work's ring parameters: `rInner` widens the inner ring, `rExtra` the
/// outer ring on top of the step, `yOff` raises the inner ring.
typedef struct {
    s16 rInner;
    s16 yOff;
    s16 rExtra;
} _ShelterR48RingScale;

void func_shelter_r48_8017FB7C(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3);
void func_shelter_r48_8017FF74(GsCOORDINATE2* arg0, s32 arg1, s32 arg2);
void func_shelter_r48_8017F124(GpEffWork* work, GsCOORDINATE2* coord, s32 part);
void func_shelter_r48_8018258C(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_r48_80180804(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3);
void func_shelter_r48_80181C14(GsCOORDINATE2* coord, s16 size, s32 yaw, s32 color);

extern u32                  Gp_LcgState;
extern SVECTOR              D_shelter_r48_8018300C;
extern u8                   D_shelter_r48_8018BE54[6][16];
extern _ShelterR48RingScale D_shelter_r48_80182FE8[];

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

/// Draws one of the ring bands of the effect as sixteen textured `POLY_FT4`
/// segments. Builds an inner and an outer 16-vertex ring in the XZ plane from
/// the work's radii plus the band's `D_shelter_r48_80182FE8` offsets, moves them
/// into world space through `coord`, then projects each segment and picks its
/// texture cell from the band's `D_shelter_r48_8018BE54` row and the work's age.
void func_shelter_r48_8017F124(GpEffWork* work, GsCOORDINATE2* coord, s32 part)
{
    void**                scratch;
    u8*                   head;
    GpBandScratch*        block;
    SVECTOR*              op;
    POLY_FT4*             prim;
    _ShelterR48RingScale* row;
    s32                   i;
    s32                   next;
    s32                   ang;
    s32                   u;
    u16                   idx;
    s16                   r0;
    s16                   r1;
    u16                   y;
    u16                   f28;

    row      = &D_shelter_r48_80182FE8[part];
    f28      = (u16)work->period;
    r1       = (u16)work->angle;
    y        = f28 + (u16)row->yOff;
    r1      += (u16)row->rInner;
    r0       = r1 + (u16)work->step + (u16)row->rExtra;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = (u8*)*scratch;
    *scratch = head - 0x118;
    block    = (GpBandScratch*)(head - 0x118);
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 16; i++) {
        ang                = i << 8;
        block->inner[i].vx = (rsin(ang) * r0) >> 12;
        block->inner[i].vy = -y;
        block->inner[i].vz = (rcos(ang) * r0) >> 12;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->inner[i]);
        gte_rtv0_real();
        gte_stsv(&block->inner[i]);
        block->inner[i].vx = *(u16*)&block->inner[i].vx + *(u16*)&coord->workm.t[0];
        block->inner[i].vy = *(u16*)&block->inner[i].vy + *(u16*)&coord->workm.t[1];
        block->inner[i].vz = *(u16*)&block->inner[i].vz + *(u16*)&coord->workm.t[2];
        block->outer[i].vx = (rsin(ang) * r1) >> 12;
        op                 = &block->inner[i] + 16;
        op->vy             = 0;
        op->vz             = (rcos(ang) * r1) >> 12;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->outer[i]);
        gte_rtv0_real();
        gte_stsv(&block->outer[i]);
        block->outer[i].vx = *(u16*)&block->outer[i].vx + *(u16*)&coord->workm.t[0];
        op->vy             = *(u16*)&op->vy + *(u16*)&coord->workm.t[1];
        op->vz             = *(u16*)&op->vz + *(u16*)&coord->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 16; i++) {
        gte_ldv0(&block->inner[i]);
        gte_rtps_real();
        gte_stsxy(&block->sxy0);
        next = (i + 1) & 0xF;
        gte_ldv3(&block->inner[next], &block->outer[i], &block->outer[next]);
        gte_rtpt_real();
        idx = (D_shelter_r48_8018BE54[part][i] + work->age) % 6;
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz);
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            prim->code = 0x2E;
            setRGB0(prim, *(u8*)&work->scale, *(u8*)&work->scale, *(u8*)&work->scale);
            prim->tpage = 0x2A;
            prim->clut  = part < 3 ? 0x42C6 : 0x4282;
            u           = idx * 0x28;
            setUV4(prim, u, 0x60, u + 0x27, 0x60, u, 0x87, u + 0x27, 0x87);
            prim->x0 = *(u16*)&block->sxy0.vx;
            prim->y0 = *(u16*)&block->sxy0.vy;
            prim->x1 = *(u16*)&block->sxy1.vx;
            prim->y1 = *(u16*)&block->sxy1.vy;
            prim->x2 = *(u16*)&block->sxy2.vx;
            prim->y2 = *(u16*)&block->sxy2.vy;
            prim->x3 = *(u16*)&block->sxy3.vx;
            prim->y3 = *(u16*)&block->sxy3.vy;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x118;
}

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

/// Projects the coordinate's world position and, if it is in front of the
/// camera, queues a semi-transparent `POLY_FT4` centred on it. `arg1` selects
/// a 32-texel column of the texture page, `arg3` is the quad's rotation and
/// `arg2` its size, divided by depth so the quad shrinks with distance.
void func_shelter_r48_8017FB7C(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    void**           scratch;
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    SVECTOR*         vec;
    s32              u0;
    s32              u1;
    s32              ang2;
    u16              vz;

    scratch                                   = (void**)G_SCRATCH_HEAD;
    head                                      = *scratch;
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = *(u16*)&arg0->workm.t[1];
    vz                                        = *(u16*)&arg0->workm.t[2];
    block->vec.vz                             = vz;
    *scratch                                  = block;
    vec                                       = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps_real();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        block->otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyFT4(prim);
        setSemiTrans(prim, 1);
        setShadeTex(prim, 1);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D3;
        u0          = arg1 << 5;
        u1          = u0 + 0x1F;
        setUV4(prim, u0, 0xE0, u1, 0xE0, u0, 0xFF, u1, 0xFF);
        block->dx = (((arg2 * 31) / block->otz) * rsin(arg3)) >> 12;
        block->dy = (((arg2 * 31) / block->otz) * rcos(arg3)) >> 12;
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        ang2      = arg3 + 0x400;
        block->dx = (((arg2 * 31) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 31) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x1C;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_r48/shelter_r48_3", func_shelter_r48_8017FF74);

INCLUDE_ASM("rooms/nonmatchings/shelter_r48/shelter_r48_3", func_shelter_r48_80180210);

void func_shelter_r48_80180804(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    u8*                head;
    RoomDraw19Scratch* block;
    POLY_FT4*          prim;
    u16                col;
    u16                row;
    s32                u0;
    s32                v0;
    s32                ang;
    s32                ang2;
    s32                bank;
    s32                idx;

    head                                        = *(u8**)G_SCRATCH_HEAD;
    ((RoomDraw19Scratch*)(head - 0x1C))->vec.vx = arg0->workm.t[0];
    *(void**)G_SCRATCH_HEAD                     = head - 0x1C;
    block                                       = *(RoomDraw19Scratch**)G_SCRATCH_HEAD;
    block->vec.vy                               = arg0->workm.t[1];
    block->vec.vz                               = arg0->workm.t[2];
    idx                                         = arg1 & 0xFFF;
    bank                                        = arg1 >> 12;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(block);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw19Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw19Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw19Scratch*)(head - 0x1C))->otz);
        block->otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2C;
        if (bank != 0) {
            prim->clut = 0x428F;
        } else {
            prim->clut = (idx & 0x3F) | 0x4380;
        }
        col = (u16)idx % 5;
        row = (u16)idx / 5;
        ang = arg3;
        u0  = col * 0x30;
        v0  = row * 0x30;
        setUV4(prim, u0, v0 + 0x70, u0 + 0x2F, v0 + 0x70, u0, v0 - 0x61, u0 + 0x2F, v0 - 0x61);
        block->dx = (((arg2 * 47) / block->otz) * rsin(ang)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        ang2      = ang + 0x400;
        block->dx = (((arg2 * 47) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *(u8**)G_SCRATCH_HEAD += 0x1C;
}

void func_shelter_r48_80180C5C(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    u8*                head;
    RoomDraw19Scratch* block;
    POLY_FT4*          prim;
    u16                col;
    u16                row;
    s32                u0;
    s32                v0;
    s32                ang;
    s32                ang2;
    s32                bank;
    u16                idx;

    head                                        = *(u8**)G_SCRATCH_HEAD;
    ((RoomDraw19Scratch*)(head - 0x1C))->vec.vx = arg0->workm.t[0];
    *(void**)G_SCRATCH_HEAD                     = head - 0x1C;
    block                                       = *(RoomDraw19Scratch**)G_SCRATCH_HEAD;
    block->vec.vy                               = arg0->workm.t[1];
    block->vec.vz                               = arg0->workm.t[2];
    idx                                         = arg1 & 0xFFF;
    bank                                        = arg1 >> 12;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(block);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw19Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw19Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw19Scratch*)(head - 0x1C))->otz);
        block->otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        if (bank != 0) {
            prim->clut = 0x428F;
        } else {
            prim->clut = 0x43D0;
        }
        col = (u16)idx % 5;
        row = (u16)idx / 5;
        ang = arg3;
        u0  = col * 0x30;
        v0  = row * 0x30;
        setUV4(prim, u0, v0 - 0x80, u0 + 0x2F, v0 - 0x80, u0, v0 - 0x51, u0 + 0x2F, v0 - 0x51);
        block->dx = (((arg2 * 47) / block->otz) * rsin(ang)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        ang2      = ang + 0x400;
        block->dx = (((arg2 * 47) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *(u8**)G_SCRATCH_HEAD += 0x1C;
}

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

/// Offsets a point (0, 0x800, 0x1400) from `coord`, turned by `yaw` about Y
/// and then by the coordinate's world matrix, and projects both the
/// coordinate's world position and that tip through `GsWSMATRIX`. When both
/// project, it draws two passes of gouraud `POLY_G4` wedges, with radii of
/// `size * 64` and `size * 128` over each end's depth: every 0x400 step
/// across the half-turn facing along the screen-space line between the points
/// queues a fan at the tip, a fan at the base and a quad joining them. `color`
/// packs `0xRGB` nibbles, each shifted into its channel's high nibble, with
/// `gDisplayState.animFrame & 1` shifted into bit 4 of every channel.
void func_shelter_r48_80181C14(GsCOORDINATE2* coord, s16 size, s32 yaw, s32 color)
{
    MATRIX                  m;
    void**                  scratch;
    u8*                     head;
    _ShelterR48BeamScratch* block;
    POLY_G4*                prim;
    s32                     pass;
    u8                      r;
    u8                      g;
    u8                      b;
    s32                     limit;
    s32                     angStart;
    s32                     scaled;
    s32                     ang;
    s32                     next;
    s32                     mid;
    s32                     blend;
    s32                     tr;
    s32                     tg;

    scratch       = (void**)G_SCRATCH_HEAD;
    head          = *scratch;
    block         = (_ShelterR48BeamScratch*)(*scratch = head - 0x2C);
    block->tip.vy = 0x800;
    block->tip.vx = 0;
    block->tip.vz = 0x1400;
    Gfx_RotMatrixY(&m, (s16)yaw, 1);
    ang = color;
    gte_SetRotMatrix(&m);
    gte_ldv0(&block->tip);
    gte_rtv0_real();
    gte_stsv(&block->tip);
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&block->tip);
    gte_rtv0_real();
    gte_stsv(&block->tip);
    block->base.vx = coord->workm.t[0];
    block->base.vy = coord->workm.t[1];
    block->base.vz = coord->workm.t[2];
    block->tip.vx += block->base.vx;
    block->tip.vy += block->base.vy;
    block->tip.vz += block->base.vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->base);
    gte_rtps_real();
    gte_stsxy(&block->sx0);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(&block->tip);
        gte_rtps_real();
        gte_stsxy(&block->sx1);
        gte_stflg(&block->flag);
        gte_stszotz(&block->otz1);
        if (block->flag >= 0) {
            tr    = (((u16)ang >> 8) & 0xF) << 4;
            tg    = (((u16)ang >> 4) & 0xF) << 4;
            blend = (*(u8*)&gDisplayState.animFrame & 1) << 4;
            r     = tr + blend;
            g     = tg + blend;
            // Dead: never read before the loop sets it, but it has to be
            // here for `b` to read `color` rather than the copy in `ang`.
            ang = 0;
            b   = ((color & 0xF) << 4) + blend;
            for (pass = 1; pass < 3; pass++) {
                scaled    = size * (pass << 6);
                block->r0 = scaled / block->otz0;
                block->r1 = scaled / block->otz1;
                ang       = (s16)ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
                if (ang < ang + 0x800) {
                    angStart = ang;
                    limit    = ang + 0x800;
                    do {
                        prim           = (POLY_G4*)gGpuPrimCursor;
                        gGpuPrimCursor = prim + 1;
                        setPolyG4(prim);
                        setRGB0(prim, 0, 0, 0);
                        setRGB1(prim, 0, 0, 0);
                        setRGB2(prim, r, g, b);
                        setRGB3(prim, 0, 0, 0);
                        prim->x0 = block->sx1 + ((block->r1 * rsin(ang + 0x800)) >> 12);
                        prim->y0 = block->sy1 + ((block->r1 * rcos(ang + 0x800)) >> 12);
                        prim->x1 = block->sx1 + ((block->r1 * rsin(ang + 0xA00)) >> 12);
                        prim->y1 = block->sy1 + ((block->r1 * rcos(ang + 0xA00)) >> 12);
                        prim->x2 = block->sx1;
                        prim->y2 = block->sy1;
                        prim->x3 = block->sx1 + ((block->r1 * rsin(ang + 0xC00)) >> 12);
                        prim->y3 = block->sy1 + ((block->r1 * rcos(ang + 0xC00)) >> 12);
                        addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                                prim);
                        Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);

                        prim           = (POLY_G4*)gGpuPrimCursor;
                        gGpuPrimCursor = prim + 1;
                        setPolyG4(prim);
                        setRGB0(prim, 0, 0, 0);
                        setRGB1(prim, 0, 0, 0);
                        setRGB2(prim, r, g, b);
                        setRGB3(prim, 0, 0, 0);
                        prim->x0 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
                        prim->y0 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
                        prim->x1 = block->sx0 + ((block->r0 * rsin(ang + 0x200)) >> 12);
                        prim->y1 = block->sy0 + ((block->r0 * rcos(ang + 0x200)) >> 12);
                        next     = ang + 0x400;
                        prim->x2 = block->sx0;
                        prim->y2 = block->sy0;
                        prim->x3 = block->sx0 + ((block->r0 * rsin(next)) >> 12);
                        prim->y3 = block->sy0 + ((block->r0 * rcos(next)) >> 12);
                        addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                                prim);
                        Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                        prim           = (POLY_G4*)gGpuPrimCursor;
                        mid            = angStart + (ang - angStart) * 2;
                        gGpuPrimCursor = prim + 1;
                        setPolyG4(prim);
                        setRGB0(prim, 0, 0, 0);
                        setRGB1(prim, 0, 0, 0);
                        setRGB2(prim, r, g, b);
                        setRGB3(prim, r, g, b);
                        prim->x0 = block->sx0 + ((block->r0 * rsin(mid)) >> 12);
                        prim->y0 = block->sy0 + ((block->r0 * rcos(mid)) >> 12);
                        prim->x1 = block->sx1 + ((block->r1 * rsin(mid)) >> 12);
                        prim->y1 = block->sy1 + ((block->r1 * rcos(mid)) >> 12);
                        prim->x2 = block->sx0;
                        prim->y2 = block->sy0;
                        prim->x3 = block->sx1;
                        prim->y3 = block->sy1;
                        addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                                prim);
                        Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                        ang = next;
                    } while (ang < limit);
                }
            }
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x2C;
}

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
