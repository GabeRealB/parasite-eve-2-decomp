#include "common.h"
#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/mem.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b6_growth_room.h"

#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")
#define gte_rtps_real()  __asm__ volatile("nop; nop; .word 0x4A180001")

extern u32 Gp_LcgState;

void func_shelter_b6_growth_room_8017E7F0(GsCOORDINATE2* coord, u16 arg1, s16 arg2, u16 arg3);
void func_shelter_b6_growth_room_8017ED28(GsCOORDINATE2* coord, u16 arg1, s16 arg2, s16 arg3);

void func_shelter_b6_growth_room_8017E448(s16 arg0, s16 arg1)
{
    POLY_G4* prim;

    prim           = (POLY_G4*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(prim + 1);
    setPolyG4(prim);
    setRGB0(prim, 0, 0, 0);
    setRGB1(prim, 0, 0, 0);
    setRGB2(prim, arg1, arg1, arg1);
    setRGB3(prim, arg1, arg1, arg1);
    setXY4(prim, -160, 120 - arg0, 160, 120 - arg0, -160, 120, 160, 120);
    addPrim((u_long*)((((u32)(0x40 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
    Gp_AddTpageShift((P_TAG*)prim, 1, 0x40);
}

void func_shelter_b6_growth_room_8017E564(Task* task)
{
    RoomEffWork*   work  = task->spawnArg2;
    GsCOORDINATE2* coord = ((TmdObject*)task->extra)->coords;
    s32            vz;
    s32            t;
    s16            f2a;
    u32            rng2;
    u32            rng3;

    work->field_22++;
    if (task->state == 0) {
        work->field_24 = (*(u16*)&task->spawnArg1) & 0xFFF;

        if (task->spawnArg1 & 0xF000) {
            work->field_28 = (task->spawnArg1 >> 12) & 0x7;
        } else {
            work->field_28 = 1;
        }

        work->field_22 = 0;
        task->state    = 1;

        if (task->spawnArg1 & 0xFF0000) {
            f2a = (task->spawnArg1 >> 16) & 0xFF;
        } else {
            f2a = 0x40;
        }

        work->field_2A    = f2a;
        work->field_10.vy = 0;
        rng2              = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState       = rng2;
        work->field_10.vx = 0x80 - ((rng2 >> 16) & 0xFF);
        rng3              = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState       = rng3;
        vz                = 0x80 - ((rng3 >> 16) & 0xFF);
        work->field_10.vz = vz;
        VectorNormalSS(&work->field_10, &work->field_10);

        gte_lddp(work->field_2A);
        gte_ldsv(&work->field_10);
        gte_gpf12_real();
        gte_stsv(&work->field_10);
    }

    if ((s16)work->field_22 < (s16)work->field_28 * 10 - 16) {
        if ((s16)work->field_26 < 0x40) {
            work->field_26 += 4;
        }
    } else {
        t              = (s16)work->field_22 + 16;
        work->field_26 = 0x40 - (t - (s16)work->field_28 * 10) * 4;
    }

    func_shelter_b6_growth_room_8017E7F0(coord, work->field_20, (s16)work->field_24, work->field_26);

    coord->coord.t[0] += work->field_10.vx;
    coord->coord.t[1] += work->field_10.vy;
    coord->coord.t[2] += work->field_10.vz;
    coord->flg         = 0;

    if (((s16)work->field_22 % (s16)work->field_28) == 0) {
        work->field_20++;
        if ((s16)work->field_20 >= 0xA) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

void func_shelter_b6_growth_room_8017E7F0(GsCOORDINATE2* coord, u16 arg1, s16 arg2, u16 arg3)
{
    void**             scratch;
    u8*                head;
    RoomDraw27Scratch* block;
    POLY_FT4*          prim;
    SVECTOR*           vec;
    s32                u0;
    s32                v0;
    s32                u1;
    s32                v1;
    s16                xy;
    u16                vz;

    scratch                                     = (void**)G_SCRATCH_HEAD;
    head                                        = *scratch;
    ((RoomDraw27Scratch*)(head - 0x1C))->vec.vx = *(u16*)&coord->workm.t[0];
    block                                       = (RoomDraw27Scratch*)(head - 0x1C);
    block->vec.vy                               = *(u16*)&coord->workm.t[1];
    vz                                          = *(u16*)&coord->workm.t[2];
    *scratch                                    = block;
    block->vec.vz                               = vz;
    vec                                         = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw27Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw27Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw27Scratch*)(head - 0x1C))->otz);
        if (block->otz >= 0x41) {
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = (u8*)(prim + 1);
            setlen(prim, 9);
            setcode(prim, 0x2C);
            prim->tpage = 0x2C;
            prim->clut  = 0x4386;
            u0          = (arg1 & 1) << 7;
            v0          = (arg1 >> 1) << 5;
            u1          = u0 + 0x7F;
            v1          = v0 + 0x1F;
            setRGB0(prim, arg3, arg3, arg3);
            setUV4(prim, u0, v0, u1, v0, u0, v1, u1, v1);
            setSemiTrans(prim, 1);
            block->dx = (arg2 * 127) / block->otz;
            block->dy = (arg2 * 31) / block->otz;
            xy        = *(u16*)&block->sx - *(u16*)&block->dx;
            prim->x2  = xy;
            prim->x0  = xy;
            xy        = *(u16*)&block->sx + *(u16*)&block->dx;
            prim->x3  = xy;
            prim->x1  = xy;
            xy        = *(u16*)&block->sy - *(u16*)&block->dy;
            prim->y1  = xy;
            prim->y0  = xy;
            xy        = *(u16*)&block->sy + *(u16*)&block->dy;
            prim->y3  = xy;
            prim->y2  = xy;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

void func_shelter_b6_growth_room_8017EAC8(Task* task)
{
    RoomEffWork*   work  = task->spawnArg2;
    GsCOORDINATE2* coord = ((TmdObject*)task->extra)->coords;
    s32            vz;
    s16            f2a;
    u32            rng2;
    u32            rng3;

    work->field_22++;
    if (task->state == 0) {
        work->field_24 = (*(u16*)&task->spawnArg1) & 0xFFF;
        Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
        work->field_26 = (Gp_LcgState >> 16) & 0xFFF;

        if (task->spawnArg1 & 0xF000) {
            work->field_28 = (task->spawnArg1 >> 12) & 0x7;
        } else {
            work->field_28 = 1;
        }

        work->field_22 = 0;
        task->state    = 1;

        if (task->spawnArg1 & 0xFF0000) {
            f2a = (task->spawnArg1 >> 16) & 0xFF;
        } else {
            f2a = 0x40;
        }

        work->field_2A    = f2a;
        work->field_10.vy = 0;
        rng2              = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState       = rng2;
        work->field_10.vx = ((rng2 >> 16) & 0x7F) + 0x40;
        rng3              = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState       = rng3;
        vz                = 0x40 - ((rng3 >> 16) & 0x7F);
        work->field_10.vz = vz;
        VectorNormalSS(&work->field_10, &work->field_10);

        gte_lddp(work->field_2A);
        gte_ldsv(&work->field_10);
        gte_gpf12_real();
        gte_stsv(&work->field_10);
    }

    func_shelter_b6_growth_room_8017ED28(coord, work->field_20, (s16)work->field_24, (s16)work->field_26);

    coord->coord.t[0] += work->field_10.vx;
    coord->coord.t[1] += work->field_10.vy;
    coord->coord.t[2] += work->field_10.vz;
    coord->flg         = 0;
    work->field_10.vy += 2;

    if (((s16)work->field_22 % (s16)work->field_28) == 0) {
        work->field_20++;
        if ((s16)work->field_20 >= 0xA) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

void func_shelter_b6_growth_room_8017ED28(GsCOORDINATE2* coord, u16 arg1, s16 arg2, s16 arg3)
{
    void**             scratch;
    u8*                head;
    RoomDraw27Scratch* block;
    POLY_FT4*          prim;
    SVECTOR*           vec;
    s32                u0;
    s32                v0;
    s32                u1;
    s32                v1;
    s32                ang;
    s32                ang2;
    u16                vz;

    scratch                                     = (void**)G_SCRATCH_HEAD;
    head                                        = *scratch;
    ((RoomDraw27Scratch*)(head - 0x1C))->vec.vx = *(u16*)&coord->workm.t[0];
    block                                       = (RoomDraw27Scratch*)(head - 0x1C);
    block->vec.vy                               = *(u16*)&coord->workm.t[1];
    vz                                          = *(u16*)&coord->workm.t[2];
    *scratch                                    = block;
    block->vec.vz                               = vz;
    vec                                         = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw27Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw27Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw27Scratch*)(head - 0x1C))->otz);
        if (block->otz >= 0x41) {
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = (u8*)(prim + 1);
            setlen(prim, 9);
            setcode(prim, 0x2F);
            prim->tpage = 0x2B;
            prim->clut  = 0x4384;
            u0          = (arg1 % 5) * 0x30;
            v0          = (arg1 / 5) * 0x30;
            u1          = u0 + 0x2F;
            v1          = v0 + 0x57;
            v0          = v0 + 0x28;
            setUV4(prim, u0, v0, u1, v0, u0, v1, u1, v1);
            ang       = arg3;
            block->dx = (((arg2 * 47) / block->otz) * rsin(ang)) >> 12;
            block->dy = (((arg2 * 47) / block->otz) * rcos(ang)) >> 12;
            prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
            prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
            prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
            ang2      = ang + 0x400;
            prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
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
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}
