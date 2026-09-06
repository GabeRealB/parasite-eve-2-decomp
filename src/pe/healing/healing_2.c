#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

/// Per-level scale row. `field_2` is the starting radius; after frame 0x10 it
/// decays by that value >> 4. Indexed by `(Gp_StateC08.field_0 % 10) - 1`.
typedef struct HealingScale {
    /* 0x0 */ s16 unk0;
    /* 0x2 */ u16 field_2;
    /* 0x4 */ s16 unk4;
    /* 0x6 */ s16 unk6;
} HealingScale;
STATIC_ASSERT_SIZEOF(HealingScale, 8);

extern HealingScale D_healing_8012FC1C[];

void func_healing_8012F7FC(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, s16 arg3);

void func_healing_8012F5E4(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s32            y;
    s16            step;
    s16            kind;
    GpEffWork*     spawned;

    mem           = arg0->spawnArg2;
    coord         = ((TmdObject*)arg0->extra)->field_8;
    mem->field_22 = (u16)mem->field_22 + 1;
    if (arg0->state == 0) {
        coord->sub        = mem->field_8;
        coord->coord.t[0] = mem->field_18;
        coord->coord.t[1] = mem->field_1A;
        coord->coord.t[2] = mem->field_1C;
        coord->flg        = 0;
        Gp_UpdateCoord(coord);
        mem->field_12 = 4;
        mem->field_10 = 0;
        mem->field_14 = 0;
        arg0->state   = 1;
        kind          = (Gp_StateC08.field_0 % 10U) - 1;
        mem->field_2A = kind;
        mem->field_24 = D_healing_8012FC1C[kind].field_2;
        mem->field_26 = (u16)arg0->spawnArg1 & 0xFFF;
    }
    step              = mem->field_12;
    y                 = coord->coord.t[1] + step;
    coord->flg        = 0;
    coord->coord.t[1] = y;
    Gp_UpdateCoord(coord);
    if (mem->field_22 < 0x1E) {
        if ((u16)mem->field_22 & 1) {
            mem->field_20 = (u16)mem->field_20 + 1;
            if (mem->field_22 >= 0x10) {
                mem->field_24 = (u16)mem->field_24 - ((s16)D_healing_8012FC1C[mem->field_2A].field_2 >> 4);
            }
            if (mem->field_2A < 2) {
                func_800EB6E8(coord, (u16)mem->field_20, (u16)mem->field_26,
                              (u16)mem->field_24);
            } else {
                func_healing_8012F7FC(coord, mem->field_20, mem->field_26, mem->field_24);
            }
            if (((u16)mem->field_22 & 7) == 1) {
                spawned = Gp_SpawnEff(0x60016, coord, mem->field_26, 0);
                if (spawned != NULL) {
                    Task_Reparent(arg0, spawned->field_0);
                }
            }
        }
    } else {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}

/// Links the two quads of one healing pulse. `arg0`'s world position is
/// projected through `GsWSMATRIX` by a single `RTPS` and both quads are
/// dropped when that sets a negative `gte_stflg`. The inner quad takes one of
/// the four 0x18-wide frames on tpage 0x2A (CLUT 0x42C5) picked by
/// `arg1 & 3`, is tinted `arg3` and sits `arg2 * 23 / otz` from the projected
/// centre; the outer glow takes the single 0x38..0x6F cell on tpage 0x29 with
/// the CLUT alternating on `arg1 & 1`, is tinted `arg3 / 2` and sits
/// `(arg2 / 2) * 55 / otz` out. Both are axis-aligned and linked into
/// `Gpu_CurrentOt` at the shared `otz`. Same 0x18-byte scratch as gameplay
/// `Gp_EffSprTask8D`.
void func_healing_8012F7FC(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    void**           scratch;
    u8*              head;
    GpEffFt4Scratch* block;
    POLY_FT4*        prim;
    SVECTOR*         vec;
    s32              u0;
    s32              u1;
    s16              x;
    s16              y;
    u16              vz;

    scratch                                   = (void**)G_SCRATCH_HEAD;
    head                                      = *scratch;
    ((GpEffFt4Scratch*)(head - 0x18))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                     = (GpEffFt4Scratch*)(head - 0x18);
    block->vec.vy                             = *(u16*)&arg0->workm.t[1];
    vz                                        = *(u16*)&arg0->workm.t[2];
    *scratch                                  = block;
    block->vec.vz                             = vz;
    vec                                       = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps_real();
    gte_stsxy(&((GpEffFt4Scratch*)(head - 0x18))->sx);
    gte_stflg(&((GpEffFt4Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpEffFt4Scratch*)(head - 0x18))->otz);
        block->otz++;
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2E);
        prim->tpage = 0x2A;
        prim->clut  = 0x42C5;
        u0          = (arg1 & 3) * 0x18;
        u1          = u0 + 0x17;
        prim->u0    = u0;
        prim->u1    = u1;
        prim->u2    = u0;
        prim->u3    = u1;
        prim->v2    = 0x17;
        prim->v3    = 0x17;
        setRGB0(prim, arg3, arg3, arg3);
        prim->v0    = 0;
        prim->v1    = 0;
        block->size = (arg2 * 0x17) / block->otz;
        x           = *(u16*)&block->sx - *(u16*)&block->size;
        prim->x2    = x;
        prim->x0    = x;
        x           = *(u16*)&block->sx + *(u16*)&block->size;
        prim->x3    = x;
        prim->x1    = x;
        y           = *(u16*)&block->sy - *(u16*)&block->size;
        prim->y1    = y;
        prim->y0    = y;
        y           = *(u16*)&block->sy + *(u16*)&block->size;
        prim->y3    = y;
        prim->y2    = y;
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) +
                          (s32)Gpu_CurrentOt),
                prim);

        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        prim->tpage    = 0x29;
        prim->clut     = ((u32)(((arg1 & 1) * 0x10) + 0x100) >> 4) | 0x4300;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        arg3 = arg3 >> 1;
        setRGB0(prim, arg3, arg3, arg3);
        setUV4(prim, 0x38, 0xC8, 0x6F, 0xC8, 0x38, 0xFF, 0x6F, 0xFF);
        block->size = ((arg2 >> 1) * 0x37) / block->otz;
        x           = *(u16*)&block->sx - *(u16*)&block->size;
        prim->x2    = x;
        prim->x0    = x;
        x           = *(u16*)&block->sx + *(u16*)&block->size;
        prim->x3    = x;
        prim->x1    = x;
        y           = *(u16*)&block->sy - *(u16*)&block->size;
        prim->y1    = y;
        prim->y0    = y;
        y           = *(u16*)&block->sy + *(u16*)&block->size;
        prim->y3    = y;
        prim->y2    = y;
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) +
                          (s32)Gpu_CurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x18;
}
