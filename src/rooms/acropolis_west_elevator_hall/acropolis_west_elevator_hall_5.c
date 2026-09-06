#include "common.h"

#include "gameplay/3CD8.h"

#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/acropolis_west_elevator_hall.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

INCLUDE_ASM("rooms/nonmatchings/acropolis_west_elevator_hall/acropolis_west_elevator_hall_5", func_acropolis_west_elevator_hall_8017FE18);

/// Draws one frame of the hall's soft light billboard and then retires the
/// task. The effect coordinate is projected through the identity world matrix
/// with a single `RTPS`, and the resulting screen point becomes the centre of
/// a semi-transparent `POLY_FT4` whose half-extent shrinks with distance
/// (`0x6700 / otz`). Sprites closer than `otz == 0x11` are skipped entirely,
/// which is why the primitive is claimed from `Gpu_PrimCursor` before the
/// depth test but only filled in and linked afterwards.
void func_acropolis_west_elevator_hall_8017FFE4(Task* arg0)
{
    void**             scratch;
    u8*                head;
    AwehSpriteScratch* block;
    s32*               otzp;
    GsCOORDINATE2*     coord;
    void*              mem;
    POLY_FT4*          prim;
    u16                vz;

    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    mem   = arg0->spawnArg2;
    Gp_UpdateCoord(coord);
    scratch       = (void**)G_SCRATCH_HEAD;
    head          = *scratch;
    block         = (AwehSpriteScratch*)(head - 0x14);
    otzp          = &block->otz;
    block->pos.vx = *(u16*)&coord->workm.t[0];
    block->pos.vy = *(u16*)&coord->workm.t[1];
    vz            = *(u16*)&coord->workm.t[2];
    *scratch      = block;
    block->pos.vz = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((AwehSpriteScratch*)(head - 0x14))->pos);
    gte_rtps_real();
    prim           = (POLY_FT4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&((AwehSpriteScratch*)(head - 0x14))->sx);
    gte_stszotz(otzp);
    if (((AwehSpriteScratch*)(head - 0x14))->otz >= 0x11) {
        prim->tpage   = 0xAB;
        prim->clut    = 0x4380;
        prim->u0      = 0;
        prim->v0      = 0;
        prim->u1      = 0x67;
        prim->v1      = 0;
        prim->u2      = 0;
        prim->v2      = 0x67;
        prim->u3      = 0x67;
        prim->v3      = 0x67;
        prim->code   |= 3;
        block->radius = 0x6700 / ((AwehSpriteScratch*)(head - 0x14))->otz;
        prim->x0 = prim->x2 = block->sx - *(u16*)&block->radius;
        prim->x1 = prim->x3 = block->sx + *(u16*)&block->radius;
        prim->y0 = prim->y1 = block->sy - *(u16*)&block->radius;
        prim->y2 = prim->y3 = block->sy + *(u16*)&block->radius;
        addPrim((u_long*)(((((u32)((AwehSpriteScratch*)(head - 0x14))->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    Gp_ReleaseState1CMem(mem, arg0);
}

s32 func_acropolis_west_elevator_hall_80180274(void)
{
    Gp_SpawnEff(0x60033, NULL, 0, NULL);
    return 0;
}
