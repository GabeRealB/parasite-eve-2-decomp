#include "common.h"

#include <psyq/inline_c.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

extern s32 Gp_LcgState;

/// `rtps`. The `inline_c.h` macro of that name assembles to a different word,
/// so spell the instruction out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

void func_pyrokinesis_8012FC34(GsCOORDINATE2* arg0, s16 arg1, s16 arg2);
void func_pyrokinesis_80130DC0(GsCOORDINATE2* arg0, s16 arg1, s32 arg2, s16 arg3);
void func_pyrokinesis_801312B4(GsCOORDINATE2* arg0, s16 arg1, s32 arg2, s16 arg3);

INCLUDE_RODATA("pe/nonmatchings/pyrokinesis/pyrokinesis", D_pyrokinesis_8012EF30);

INCLUDE_ASM("pe/nonmatchings/pyrokinesis/pyrokinesis", func_pyrokinesis_8012EF48);

void func_pyrokinesis_8012FAC8(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            scene;
    s16            flag;
    s32            state;

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if (Gp_StateC08.field_3 != -2) {
        scene = Gp_State1C->field_16;
        if (scene == 1) {
            flag = Gp_State1C->field_E;
            if (flag < 4) {
                if (flag != 0) {
                    return;
                }
                mem->field_22 = (u16)mem->field_22 + 1;
                Gp_UpdateCoord(coord);
                state = arg0->state;
                if (state == scene) {
                    goto L_case1;
                }
                if (state < 2) {
                    if (state == 0) {
                        goto L_case0;
                    }
                    return;
                }
                if (state == 2) {
                    goto L_case2;
                }
                if (state == 3) {
                    goto L_release;
                }
                return;
            L_case0:
                Gp_SpawnEff(0x80060010, coord, 0, 0);
                arg0->state = scene;
                return;
            L_case1:
                if (mem->field_22 == 8) {
                    Gp_SpawnEff(0x80060010, coord, 1, 0);
                    arg0->state = 2;
                }
                return;
            L_case2:
                if (mem->field_22 == 0x10) {
                    Gp_SpawnEff(0x80060000 | 0x10, coord, 2, 0);
                    arg0->state = 3;
                }
                return;
            }
        }
    }
L_release:
    Gp_ReleaseState1CMem(mem, arg0);
}

INCLUDE_ASM("pe/nonmatchings/pyrokinesis/pyrokinesis", func_pyrokinesis_8012FC34);

/// Draws the flame ring: `arg0`'s origin is projected once through
/// `GsWSMATRIX` and eight `POLY_G4` blades are swept around it, each spanning
/// a 0x200 arc of radius `(arg1 * 64) / otz`. Only the third vertex carries
/// colour, the rest of the blade fading to black, and that colour is the
/// `arg2` ramp `(arg2, arg2 >> 1, arg2 >> 2)` - a red-biased fire tint. A
/// negative `gte_stflg` drops the whole ring.
void func_pyrokinesis_80130130(GsCOORDINATE2* arg0, s32 arg1, s16 arg2)
{
    void**         scratch;
    u8*            head;
    GpRingScratch* block;
    POLY_G4*       prim;
    s32            ang;
    register s32   ang2 asm("s1");
    u16            vz;
    u16            red;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    USE_REG(head);
    {
        register u16 vx asm("v0");
        vx                                      = *(u16*)&arg0->workm.t[0];
        ((GpRingScratch*)(head - 0x18))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x18;
        block = (GpRingScratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;
    red           = arg2;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
        USE_REG(head);
        block->otz++;
        block->step = ((s16)arg1 * 64) / block->otz;
        ang         = 0;
        do {
            prim           = (POLY_G4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, *(u8*)&red, arg2 >> 1, arg2 >> 2);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->step * rsin(ang)) >> 12);
            prim->y0 = *(u16*)&block->sy + ((block->step * rcos(ang)) >> 12);
            ang2     = ang + 0x100;
            prim->x1 = *(u16*)&block->sx + ((block->step * rsin(ang2)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->step * rcos(ang2)) >> 12);
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            ang2     = ang + 0x200;
            prim->x3 = *(u16*)&block->sx + ((block->step * rsin(ang2)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->step * rcos(ang2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) +
                              (s32)Gpu_CurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            ang = ang2;
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

INCLUDE_ASM("pe/nonmatchings/pyrokinesis/pyrokinesis", func_pyrokinesis_801304C4);

INCLUDE_ASM("pe/nonmatchings/pyrokinesis/pyrokinesis", func_pyrokinesis_80130848);

void func_pyrokinesis_80130C54(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;
    s16            temp_a1;
    s32            y;

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if (Gp_StateC08.field_3 != -2) {
        flag = Gp_State1C->field_E;
        if (flag < 4) {
            if (flag != 0) {
                return;
            }
            mem->field_22 = (u16)mem->field_22 + 1;
            if (arg0->state == 0) {
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_12 = -(((u32)Gp_LcgState >> 16) & 0x1F);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_24 = ((u32)Gp_LcgState >> 16) & 0xFFF;
                arg0->state   = 1;
            }
            y                 = coord->coord.t[1] + mem->field_12;
            coord->flg        = 0;
            coord->coord.t[1] = y;
            Gp_UpdateCoord(coord);
            if (!((u16)mem->field_22 & 1)) {
                mem->field_20 = (u16)mem->field_20 + 1;
            }
            temp_a1 = mem->field_20;
            if (temp_a1 < 8) {
                if ((u16)mem->field_22 & 1) {
                    func_pyrokinesis_80130DC0(coord, temp_a1, 0x300, mem->field_24);
                }
                return;
            }
        }
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

INCLUDE_ASM("pe/nonmatchings/pyrokinesis/pyrokinesis", func_pyrokinesis_80130DC0);

void func_pyrokinesis_801311B8(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;
    s32            scale;
    s32            angle;

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if (Gp_StateC08.field_3 != -2) {
        flag = Gp_State1C->field_E;
        if (flag < 4) {
            if (flag != 0) {
                return;
            }
            if (arg0->state == 0) {
                Gfx_RotMatrixZ(&coord->coord, arg0->spawnArg1, 0);
                coord->flg    = 0;
                mem->field_24 = 0x80;
                mem->field_26 = 0x100;
                arg0->state   = 1;
            }
            Gp_UpdateCoord(coord);
            func_pyrokinesis_801312B4(coord, mem->field_26, 0x100, mem->field_24);
            angle         = (u16)mem->field_26;
            scale         = (u16)mem->field_24;
            angle        += 0x80;
            scale        -= 8;
            mem->field_24 = scale;
            mem->field_26 = angle;
            if ((s16)scale >= 9) {
                return;
            }
        }
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

/// Draws the pyrokinesis flame band: two 16-vertex rings of radius `arg1`
/// and `arg1 + arg2` are built in the XZ plane by `rsin` / `rcos`, rotated by
/// `arg0`'s `workm` and offset by its translation, then each of the 16
/// segments is projected through `GsWSMATRIX` as one `POLY_G4`. The inner
/// edge carries the `arg3` ramp `(arg3, arg3 >> 1, arg3 >> 2)` and the outer
/// edge fades to black; a negative `gte_stflg` drops the segment.
void func_pyrokinesis_801312B4(GsCOORDINATE2* arg0, s16 arg1, s32 arg2, s16 arg3)
{
    void**         scratch;
    register u8*   head asm("v0");
    GpBandScratch* block;
    SVECTOR*       op;
    POLY_G4*       prim;
    s32            i;
    s32            next;
    s32            ang;
    s16            r0;
    s16            r1;

    r1       = arg1 + arg2;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = (u8*)*scratch - 0x118;
    block    = (GpBandScratch*)head;
    *scratch = head;
    gte_SetTransMatrix(&GsWSMATRIX);
    r0 = arg1;
    for (i = 0; i < 16; i++) {
        ang                = i << 8;
        block->inner[i].vx = (rsin(ang) * r0) >> 12;
        block->inner[i].vy = 0;
        block->inner[i].vz = (rcos(ang) * r0) >> 12;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->inner[i]);
        gte_rtv0_real();
        gte_stsv(&block->inner[i]);
        block->inner[i].vx = *(u16*)&block->inner[i].vx + *(u16*)&arg0->workm.t[0];
        block->inner[i].vy = *(u16*)&block->inner[i].vy + *(u16*)&arg0->workm.t[1];
        block->inner[i].vz = *(u16*)&block->inner[i].vz + *(u16*)&arg0->workm.t[2];
        block->outer[i].vx = (rsin(ang) * r1) >> 12;
        op                 = &block->inner[i] + 16;
        op->vy             = 0;
        op->vz             = (rcos(ang) * r1) >> 12;
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
        gte_stsxy(&block->sxy0);
        next = (i + 1) & 0xF;
        gte_ldv3(&block->inner[next], &block->outer[i], &block->outer[next]);
        gte_rtpt_real();
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz);
            block->otz++;
            prim           = (POLY_G4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setPolyG4(prim);
            setRGB0(prim, *(u8*)&arg3, arg3 >> 1, arg3 >> 2);
            setRGB1(prim, *(u8*)&arg3, arg3 >> 1, arg3 >> 2);
            setRGB2(prim, 0, 0, 0);
            setRGB3(prim, 0, 0, 0);
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
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x118;
}

INCLUDE_ASM("pe/nonmatchings/pyrokinesis/pyrokinesis", func_pyrokinesis_80131784);

void func_pyrokinesis_80131CE4(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;
    s32            scale;
    s32            angle;

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if (Gp_StateC08.field_3 != -2) {
        flag = Gp_State1C->field_E;
        if (flag < 4) {
            if (flag != 0) {
                return;
            }
            mem->field_22 = (u16)mem->field_22 + 1;
            if (arg0->state == 0) {
                mem->field_24 = 0xC0;
                mem->field_26 = 0x100;
                arg0->state   = 1;
            }
            Gp_UpdateCoord(coord);
            func_pyrokinesis_8012FC34(((TmdObject*)arg0->extra)->field_8, mem->field_26, mem->field_24);
            angle         = (u16)mem->field_26;
            scale         = (u16)mem->field_24;
            angle        += 0x40;
            scale        -= 0x10;
            mem->field_24 = scale;
            mem->field_26 = angle;
            if ((s16)scale >= 0x10) {
                return;
            }
        }
    }
    Gp_ReleaseState1CMem(mem, arg0);
}
