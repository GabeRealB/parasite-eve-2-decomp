#include "common.h"

#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "pe/combustion.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

/// Links one frame of the combustion flame at `arg0`'s world position. The
/// position is projected through `GsWSMATRIX` by a single `RTPS` and the quad
/// is dropped when that sets a negative `gte_stflg`. `arg1` picks one of the
/// eight 0x18-wide texture frames on tpage 0x28 (CLUT 0x430D), and `arg2`
/// sizes it: the corners sit `arg2 * 23 / otz` from the projected centre, so
/// the sprite shrinks with depth. Same 0x18-byte scratch and axis-aligned
/// quad as gameplay `Gp_EffSprTask8D`.
void func_combustion_8012FF0C(GsCOORDINATE2* arg0, s32 arg1, s16 arg2)
{
    void**           scratch;
    u8*              head;
    GpEffFt4Scratch* block;
    POLY_FT4*        prim;
    SVECTOR*         vec;
    s32              u0;
    s32              u1;
    s32              va;
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
        setcode(prim, 0x2F);
        prim->tpage = 0x28;
        prim->clut  = 0x430D;
        u0          = (arg1 & 7) * 0x18;
        va          = 0xA0;
        u1          = u0 + 0x17;
        SCHED_BARRIER();
        prim->u0 = u0;
        prim->u2 = u0;
        SCHED_BARRIER();
        prim->v0    = va;
        prim->v1    = va;
        prim->v2    = 0xB7;
        prim->v3    = 0xB7;
        prim->u1    = u1;
        prim->u3    = u1;
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
    }
    *scratch = (u8*)*scratch + 0x18;
}

/// Draws one billboard quad of a combustion flame. The coordinate's world
/// position is projected through `GsWSMATRIX` with a single `RTPS`; a negative
/// `gte_stflg` drops the quad. `arg2 * 0x37` divided by the resulting `otz + 1`
/// is the on-screen half-diagonal, and `arg3` rotates it, the second diagonal
/// following a quarter turn (0x400) later, so the quad stays square but spins
/// with the flame. `arg1`'s low bit picks the frame: odd takes the tinted
/// semi-transparent core at `0x428B` / u 0x70..0xA7, even the additive outer
/// flame at `0x428C` / u 0xA8..0xDF. The quad is linked into `Gpu_CurrentOt` at
/// its own `otz` twice, once per diagonal pair.
void func_combustion_80130184(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    CombustionQuadScratch* block;
    POLY_FT4*              prim;
    s32                    ang;
    u16                    vz;
    register void**        scratch asm("a1");
    s16                    saved;
    register s32           hi asm("a1");
    u8*                    head;

    saved = arg1;
    /* `head` is a dummy output: it reserves its register from entry on, which
       is what keeps it out of the argument registers. */
    asm("lui %0, 0x1F80" : "=r"(hi), "=r"(head) : "r"(saved));
    asm("ori %0, %1, 0x3FC" : "=r"(scratch) : "r"(hi));
    head = *scratch;
    USE_REG(head);
    {
        register u16 vx asm("v0");
        vx                                              = *(u16*)&arg0->workm.t[0];
        ((CombustionQuadScratch*)(head - 0x1C))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (CombustionQuadScratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(&((CombustionQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((CombustionQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((CombustionQuadScratch*)(head - 0x1C))->otz);
        USE_REG(head);
        block->otz     = block->otz + 1;
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setPolyFT4(prim);
        if (saved & 1) {
            setRGB0(prim, 0xC0, 0x70, 0x40);
            prim->tpage = 0x29;
            prim->clut  = 0x428B;
            setUV4(prim, 0x70, 0xC8, 0xA7, 0xC8, 0x70, 0xFF, 0xA7, 0xFF);
            setSemiTrans(prim, 1);
        } else {
            setcode(prim, 0x2F);
            prim->tpage = 0x29;
            prim->clut  = 0x428C;
            setUV4(prim, 0xA8, 0xC8, 0xDF, 0xC8, 0xA8, 0xFF, 0xDF, 0xFF);
        }
        ang       = arg3;
        block->dx = (((arg2 * 0x37) / block->otz) * rsin(ang)) >> 12;
        block->dy = (((arg2 * 0x37) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        ang       = ang + 0x400;
        block->dx = (((arg2 * 0x37) / block->otz) * rsin(ang)) >> 12;
        block->dy = (((arg2 * 0x37) / block->otz) * rcos(ang)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) +
                          (s32)Gpu_CurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

/// Links one frame of the large combustion flame at `arg0`'s world position,
/// the same way `func_combustion_8012F5EC` does the small one: the position is
/// projected through `GsWSMATRIX` by a single `RTPS` and the quad is dropped
/// when that sets a negative `gte_stflg`. `arg1 % 12` picks a cell of the
/// 6x2 sheet of 0x28-pixel frames on tpage 0x2A, each with its own CLUT
/// (`0x4300` plus the cell index), and `arg2` sizes it: the corners sit
/// `arg2 * 39 / otz` from the projected centre, so the sprite shrinks with
/// depth.
void func_combustion_801305F8(GsCOORDINATE2* arg0, s16 arg1, s16 arg2)
{
    void**           scratch;
    u8*              head;
    GpEffFt4Scratch* block;
    POLY_FT4*        prim;
    SVECTOR*         vec;
    s32              frame;
    u32              cell;
    u16              col;
    u16              row;
    s32              u0;
    s32              u1;
    s32              v0;
    s32              v2;
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
    frame = arg1;
    frame = frame % 12;
    gte_stsxy(&((GpEffFt4Scratch*)(head - 0x18))->sx);
    gte_stflg(&((GpEffFt4Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpEffFt4Scratch*)(head - 0x18))->otz);
        block->otz++;
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2A;
        cell        = (u16)frame;
        prim->clut  = (cell & 0x3F) | 0x4300;
        col         = cell % 6;
        row         = cell / 6;
        u0          = col * 0x28;
        u1          = u0 + 0x27;
        v0          = row * 0x28 - 0x78;
        v2          = row * 0x28 - 0x51;
        setUV4(prim, u0, v0, u1, v0, u0, v2, u1, v2);
        block->size = (arg2 * 0x27) / block->otz;
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

void func_combustion_801308E0(Task* arg0)
{
    GsCOORDINATE2* coord;

    if (arg0->state != 0) {
        Gp_ReleaseState1CMem(arg0->spawnArg2, arg0);
        return;
    }
    coord = ((TmdObject*)arg0->extra)->field_8;
    Gp_UpdateCoord(coord);
    Gp_SpawnEff(0x8006001B, coord, 1, 0);
    Gp_SpawnEff(0x8006001B, coord, -1, 0);
    arg0->state = arg0->state + 1;
}
