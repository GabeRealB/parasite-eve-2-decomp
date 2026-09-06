#include "common.h"

#include <psyq/inline_c.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "pe/energyball.h"

extern s32 D_80115724;
extern s32 Gp_LcgState;

/// `rtps` / `rtpt` / `mvmva 1,0,0,3,0`. The `inline_c.h` macros of those names
/// assemble to different words, so spell the instructions out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

/// Fires the energy ball: on the first frame it picks the charge level from the
/// combo counter, plays the matching loop sound, refills the surface-jitter
/// table and spawns one ball per charge level, fanning them out by 0x555 of
/// yaw each while `D_80115724` (the number of balls already in flight) allows
/// it. Every later frame just releases the work block.
void func_energyball_8012EF48(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s32            i;
    s32            level;
    s32            rng;

    mem           = arg0->spawnArg2;
    coord         = ((TmdObject*)arg0->extra)->field_8;
    mem->field_22 = (u16)mem->field_22 + 1;
    switch (arg0->state) {
        case 0:
            mem->field_20 = Gp_StateC08.field_0 % 10 - 1;
            level         = mem->field_20;
            mem->field_26 = (level << 8) + 0x300;
            if (D_80115724 < 0) {
                D_80115724 = 0;
            }
            if (D_80115724 == 0) {
                SndEvt_EnqueueType6(D_energyball_8013117C[mem->field_20], 0, 0);
            }
            for (i = 0; i < 0x10; i++) {
                rng                      = Gp_LcgState * 5 + 0x71357911;
                D_energyball_801311A0[i] = ((u32)rng >> 16) & 0xFF;
                Gp_LcgState              = rng;
            }
            for (i = 0; i < mem->field_20 + 1; i++) {
                if (D_80115724 + i >= 3) {
                    break;
                }
                mem->field_24 = i * 0x555 - (s16)(u16)mem->field_20 * 0x2AA;
                mem->field_10 = (mem->field_26 * rsin(mem->field_24)) >> 12;
                mem->field_14 = (mem->field_26 * rcos(mem->field_24)) >> 12;
                Gp_SpawnEff(0x800600F8, coord, i, (SVECTOR*)&mem->field_10);
            }
            arg0->state = 1;
            return;
        case 1:
            Gp_ReleaseState1CMem(mem, arg0);
            return;
    }
}

INCLUDE_RODATA("pe/nonmatchings/energyball/energyball", D_energyball_8012EF30);

INCLUDE_ASM("pe/nonmatchings/energyball/energyball", func_energyball_8012F180);

INCLUDE_ASM("pe/nonmatchings/energyball/energyball", func_energyball_8012FFD0);

/// Links one frame of the energy ball's core sprite at `arg0`'s world
/// position. The position is projected through `GsWSMATRIX` by a single `RTPS`
/// and the quad is dropped when that sets a negative `gte_stflg`. `arg1` is the
/// effect's frame counter and its low bit alternates the two looks: odd frames
/// draw the raw, semi-transparent 0x428B cell, even frames the 0x428C cell
/// tinted `(0x40, 0xC0, 0x60)`. `arg3` spins the quad and `arg2` sizes it: the
/// corners sit `arg2 * 55 / otz` from the projected centre along `arg3` and
/// `arg3 + 0x400`, so the sprite shrinks with depth.
void func_energyball_8013035C(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    void**                      scratch;
    u8*                         head;
    EnergyQuadScratch*          block;
    register EnergyQuadScratch* p asm("v0");
    register u16                vx asm("v0");
    POLY_FT4*                   prim;
    s32                         ang2;
    u16                         vz;

    scratch                                     = (void**)G_SCRATCH_HEAD;
    head                                        = *scratch;
    vx                                          = *(u16*)&arg0->workm.t[0];
    ((EnergyQuadScratch*)(head - 0x1C))->vec.vx = vx;
    p                                           = (EnergyQuadScratch*)(head - 0x1C);
    block                                       = p;
    block->vec.vy                               = *(u16*)&arg0->workm.t[1];
    vz                                          = *(u16*)&arg0->workm.t[2];
    block->vec.vz                               = vz;
    *scratch                                    = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(&((EnergyQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((EnergyQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((EnergyQuadScratch*)(head - 0x1C))->otz);
        block->otz++;
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setPolyFT4(prim);
        if (arg1 & 1) {
            setSemiTrans(prim, 1);
            setShadeTex(prim, 1);
            prim->tpage = 0x29;
            prim->clut  = 0x428B;
            setUV4(prim, 0x70, 0xC8, 0xA7, 0xC8, 0x70, 0xFF, 0xA7, 0xFF);
        } else {
            setRGB0(prim, 0x40, 0xC0, 0x60);
            prim->tpage = 0x29;
            prim->clut  = 0x428C;
            setUV4(prim, 0xA8, 0xC8, 0xDF, 0xC8, 0xA8, 0xFF, 0xDF, 0xFF);
            setSemiTrans(prim, 1);
        }
        block->dx = (((arg2 * 55) / block->otz) * rsin(arg3)) >> 12;
        block->dy = (((arg2 * 55) / block->otz) * rcos(arg3)) >> 12;
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        ang2      = arg3 + 0x400;
        block->dx = (((arg2 * 55) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 55) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) +
                          (s32)Gpu_CurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
    DEF_REG(head);
}

INCLUDE_ASM("pe/nonmatchings/energyball/energyball", func_energyball_801307D4);

/// Draws the energy ball's surface: two 16-vertex rings of the same radius
/// sit `arg1 * 2` apart in `arg0`'s local Y, are rotated by its `workm` and
/// offset by its translation, then each of the 16 segments is projected
/// through `GsWSMATRIX` as one semi-transparent `POLY_FT4`. The texture cell
/// is one of six 0x28-wide frames picked per vertex by the jitter table
/// `D_energyball_801311A0` plus the frame counter, the quad is tinted
/// `(arg2 >> 1, arg2, arg2 >> 1)`, and a negative `gte_stflg` drops the
/// segment.
void func_energyball_80130B54(GsCOORDINATE2* arg0, s16 arg1, s16 arg2)
{
    void**         scratch;
    register u8*   head asm("v0");
    GpBandScratch* block;
    SVECTOR*       op;
    POLY_FT4*      prim;
    s32            i;
    s32            next;
    s32            ang;
    s32            u;
    s16            idx;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = (u8*)*scratch - 0x118;
    block    = (GpBandScratch*)head;
    *scratch = head;
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 16; i++) {
        ang                = i << 8;
        block->inner[i].vx = (u32)(rsin(ang) * 3) >> 5;
        block->inner[i].vy = -(arg1 * 2);
        block->inner[i].vz = (u32)(rcos(ang) * 3) >> 5;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->inner[i]);
        gte_rtv0_real();
        gte_stsv(&block->inner[i]);
        block->inner[i].vx = *(u16*)&block->inner[i].vx + *(u16*)&arg0->workm.t[0];
        block->inner[i].vy = *(u16*)&block->inner[i].vy + *(u16*)&arg0->workm.t[1];
        block->inner[i].vz = *(u16*)&block->inner[i].vz + *(u16*)&arg0->workm.t[2];
        block->outer[i].vx = (u32)(rsin(ang) * 3) >> 5;
        op                 = &block->inner[i] + 16;
        op->vy             = 0;
        op->vz             = (u32)(rcos(ang) * 3) >> 5;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->outer[i]);
        gte_rtv0_real();
        gte_stsv(&block->outer[i]);
        block->outer[i].vx = *(u16*)&block->outer[i].vx + *(u16*)&arg0->workm.t[0];
        op->vy             = *(u16*)&op->vy + *(u16*)&arg0->workm.t[1];
        op->vz             = *(u16*)&op->vz + *(u16*)&arg0->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 16; i++) {
        gte_ldv0(&block->inner[i]);
        gte_rtps_real();
        idx = (u32)(D_energyball_801311A0[i] + Display_State.field_8) % 6;
        gte_stsxy(&block->sxy0);
        next = (i + 1) & 0xF;
        gte_ldv3(&block->inner[next], &block->outer[i], &block->outer[next]);
        gte_rtpt_real();
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz);
            block->otz++;
            prim           = (POLY_FT4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setPolyFT4(prim);
            prim->tpage = 0x2A;
            prim->clut  = 0x42C1;
            u           = idx * 0x28;
            setRGB0(prim, arg2 >> 1, arg2, arg2 >> 1);
            setUV4(prim, u, 0x60, u + 0x27, 0x60, u, 0x87, u + 0x27, 0x87);
            setSemiTrans(prim, 1);
            prim->x0 = *(u16*)&block->sxy0.vx;
            prim->y0 = *(u16*)&block->sxy0.vy;
            prim->x1 = *(u16*)&block->sxy1.vx;
            prim->y1 = *(u16*)&block->sxy1.vy;
            prim->x2 = *(u16*)&block->sxy2.vx;
            prim->y2 = *(u16*)&block->sxy2.vy;
            prim->x3 = *(u16*)&block->sxy3.vx;
            prim->y3 = *(u16*)&block->sxy3.vy;
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) +
                              (s32)Gpu_CurrentOt),
                    prim);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x118;
}

void func_energyball_8013107C(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;
    u8             rgb[3];
    s32            scale;
    s32            angle;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_E;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (flag != 0) {
        return;
    }

    if (arg0->state == 0) {
        Gfx_RotMatrixZ(&coord->coord, arg0->spawnArg1 & 0xFFF, 0);
        coord->flg    = 0;
        mem->field_24 = 0x80;
        mem->field_26 = 0x100;
        arg0->state   = 1;
    }

    Gp_UpdateCoord(coord);
    rgb[0] = (u16)mem->field_24 >> 1;
    rgb[1] = *(u8*)&mem->field_24;
    rgb[2] = (u16)mem->field_24 >> 1;
    Gp_DrawBandEx(coord, mem->field_26, 0x180, rgb);

    angle         = (u16)mem->field_26;
    scale         = (u16)mem->field_24;
    angle        += 0x80;
    scale        -= 8;
    mem->field_24 = scale;
    mem->field_26 = angle;
    if ((s16)scale < 9) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}
