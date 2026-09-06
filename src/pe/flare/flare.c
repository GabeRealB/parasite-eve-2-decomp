#include "common.h"

#include <psyq/inline_c.h>

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "pe/flare.h"

extern s32 Gp_LcgState;

/// `mvmva 1, 0, 0, 3, 0`: rotate V0 by the rotation matrix, no translation.
/// The `inline_c.h` macro of that name assembles to a different word, so spell
/// the instruction out.
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

/// `rtps`: project V0 through the loaded rotation and translation matrices.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

void func_flare_8012EF34(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpStateC08*    state;
    s32            pan;
    s16            tick;
    GpEffWork*     spawned;
    s32            rng;

    state = &Gp_StateC08;
    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if ((state->field_3 == -2) || (Gp_State1C->field_E != 0)) {
        SndEvt_EnqueueType7(0xE03E0001, 1);
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    mem->field_22 = (u16)mem->field_22 + 1;
    if (arg0->state == 0) {
        pan = (s8)Gp_GetObjPan((GpObj38*)coord);
        SndEvt_EnqueueType6(0xE03E0001, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
        arg0->state = 1;
    }
    tick = mem->field_22;
    if (tick < 0x14) {
        if (tick == 8) {
            state->field_6 |= 8;
        }
        rng         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rng;
        spawned     = Gp_SpawnEff(0x6019E, coord, (((u32)rng >> 16) & 0x1FF) + 0x680, 0);
        if (spawned != NULL) {
            Task_Reparent(arg0, spawned->field_0);
        }
        return;
    }
    if (tick == 0x24) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}

void func_flare_8012F0B8(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* player;
    GpMtxWords*    dstm;
    GpMtxWords*    srcm;
    s32            rng;
    s32            temp_lo;
    s32            hi;
    s32            ang;
    s32            pitch;
    s32            rsin_arg;

    mem           = arg0->spawnArg2;
    coord         = ((TmdObject*)arg0->extra)->field_8;
    mem->field_22 = (u16)mem->field_22 + 1;
    if (arg0->state == 0) {
        player     = ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8;
        dstm       = (GpMtxWords*)&coord->coord;
        srcm       = (GpMtxWords*)&player->coord;
        dstm->w0   = srcm->w0;
        dstm->w1   = srcm->w1;
        dstm->w2   = srcm->w2;
        dstm->w3   = srcm->w3;
        dstm->h4   = srcm->h4;
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        rng = Gp_LcgState * 5 + 0x71357911;
        do {
            hi = (u32)rng >> 16;
            SCHED_BARRIER();
            ang = (u16)arg0->spawnArg1;
            TOUCH_REG(ang);
            mem->field_24 = hi & 0xFFF;
            SOFT_COMPILER_BARRIER();
            rsin_arg = mem->field_24;
            SCHED_BARRIER();
        } while (0);
        Gp_LcgState   = rng;
        ang           = ang & 0xFFF;
        mem->field_28 = ang;
        SCHED_BARRIER();
        pitch = ang;
        TOUCH_REG(pitch);
        mem->field_26 = (s32)(pitch << 16) >> 21;
        mem->field_10 = (rsin(rsin_arg) * mem->field_26) >> 12;
        temp_lo       = rcos(mem->field_24) * mem->field_26;
        mem->field_14 = 0x100;
        mem->field_12 = temp_lo >> 12;
        gte_SetRotMatrix((MATRIX*)srcm);
        gte_ldv0(&mem->field_10);
        gte_rtv0_real();
        gte_stsv(&mem->field_10);
        arg0->state = 1;
    }
    coord->coord.t[0] += mem->field_10;
    coord->coord.t[1] += mem->field_12;
    coord->coord.t[2] += mem->field_14;
    coord->flg         = 0;
    Gp_UpdateCoord(coord);
    if (!((u16)mem->field_22 & 1)) {
        mem->field_20 = (u16)mem->field_20 + 1;
    }
    if (mem->field_20 < 8) {
        func_flare_8012F304(coord, mem->field_20, mem->field_28, mem->field_24);
        return;
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

void func_flare_8012F304(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    void**            scratch;
    u8*               head;
    FlareQuadScratch* block;
    POLY_FT4*         prim;
    SVECTOR*          vec;
    s32               u0;
    s32               u1;
    s32               ang2;
    u16               vz;

    scratch                                    = (void**)G_SCRATCH_HEAD;
    head                                       = *scratch;
    ((FlareQuadScratch*)(head - 0x1C))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                      = (FlareQuadScratch*)(head - 0x1C);
    block->vec.vy                              = *(u16*)&arg0->workm.t[1];
    vz                                         = *(u16*)&arg0->workm.t[2];
    block->vec.vz                              = vz;
    *scratch                                   = block;
    vec                                        = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps_real();
    gte_stsxy(&((FlareQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((FlareQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((FlareQuadScratch*)(head - 0x1C))->otz);
        block->otz++;
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setPolyFT4(prim);
        setSemiTrans(prim, 1);
        setShadeTex(prim, 1);
        prim->tpage = 0x2A;
        prim->clut  = 0x4311;
        u0          = arg1 << 5;
        u1          = u0 + 0x1F;
        setUV4(prim, u0, 0x18, u1, 0x18, u0, 0x37, u1, 0x37);
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
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) +
                          (s32)Gpu_CurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x1C;
}

INCLUDE_RODATA("pe/nonmatchings/flare/flare", D_flare_8012EF30);
