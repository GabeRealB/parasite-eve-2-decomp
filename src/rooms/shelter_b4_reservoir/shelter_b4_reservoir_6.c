#include "common.h"

#include <psyq/inline_c.h>

#include "rooms/room_common.h"

#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

#include "main/display.h"
#include "main/mem.h"

#include "gameplay/3CD8.h"

#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")
#define gte_rtps_real()  __asm__ volatile("nop; nop; .word 0x4A180001")

extern u32 Gp_LcgState;

void func_shelter_b4_reservoir_80181668(GsCOORDINATE2* coord, u16 frame, s16 size);

void func_shelter_b4_reservoir_801813F0(Task* task)
{
    RoomEffWork*   work  = task->spawnArg2;
    GsCOORDINATE2* coord = ((TmdObject*)task->extra)->coords;
    s16            f2a;
    u32            rng;

    if (Gp_State1C->eventState != 0) {
        func_shelter_b4_reservoir_80181668(coord, work->field_20, (s16)work->field_24);
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }

    work->field_22++;
    switch (task->state) {
        case 0:
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
            work->field_10.vz = 0;
            rng               = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState       = rng;
            work->field_10.vx = -((rng >> 16) & 0x3F) - 0x40;
            VectorNormalSS(&work->field_10, &work->field_10);

            gte_lddp(work->field_2A);
            gte_ldsv(&work->field_10);
            gte_gpf12_real();
            gte_stsv(&work->field_10);
            break;
        case 1:
            func_shelter_b4_reservoir_80181668(coord, work->field_20, (s16)work->field_24);
            if ((s16)work->field_2A != 0) {
                coord->coord.t[0] += work->field_10.vx;
                coord->coord.t[1] += work->field_10.vy;
                coord->coord.t[2] += work->field_10.vz;
                coord->flg         = 0;
            }
            if (((s16)work->field_22 % (s16)work->field_28) == 0) {
                work->field_20++;
                if ((s16)work->field_20 >= 6) {
                    Gp_ReleaseState1CMem(work, task);
                }
            }
            break;
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues one semi-transparent shade-tex
/// `POLY_FT4` (tpage 0x2B, clut 0x4393). `frame` selects one of six 32-texel
/// UV columns at u = `(frame % 6) * 32 + 0x40`, v = 0x40..0x5F. `size` is a
/// half-extent; the on-screen radius is `size * 31 / otz`, and the quad is
/// axis-aligned about the projected point.
void func_shelter_b4_reservoir_80181668(GsCOORDINATE2* coord, u16 frame, s16 size)
{
    void**             scratch;
    u8*                head;
    RoomDraw14Scratch* block;
    POLY_FT4*          prim;
    SVECTOR*           vec;
    DisplayState*      ds;
    s32                col;
    s16                xy;
    u16                vz;

    scratch                                     = (void**)G_SCRATCH_HEAD;
    head                                        = *scratch;
    ((RoomDraw14Scratch*)(head - 0x18))->vec.vx = *(u16*)&coord->workm.t[0];
    block                                       = (RoomDraw14Scratch*)(head - 0x18);
    block->vec.vy                               = *(u16*)&coord->workm.t[1];
    vz                                          = *(u16*)&coord->workm.t[2];
    *scratch                                    = block;
    block->vec.vz                               = vz;
    vec                                         = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw14Scratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomDraw14Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw14Scratch*)(head - 0x18))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage   = 0x2B;
        prim->clut    = 0x4393;
        prim->v0      = 0x40;
        prim->v1      = 0x40;
        prim->v2      = 0x5F;
        prim->v3      = 0x5F;
        col           = (u16)(frame % 6) << 5;
        prim->u0      = col + 0x40;
        prim->u2      = col + 0x40;
        prim->u1      = col + 0x5F;
        prim->u3      = col + 0x5F;
        block->radius = (size * 31) / block->otz;
        xy            = *(u16*)&block->sx - *(u16*)&block->radius;
        prim->x2      = xy;
        prim->x0      = xy;
        xy            = *(u16*)&block->sx + *(u16*)&block->radius;
        prim->x3      = xy;
        prim->x1      = xy;
        xy            = *(u16*)&block->sy - *(u16*)&block->radius;
        prim->y1      = xy;
        prim->y0      = xy;
        xy            = *(u16*)&block->sy + *(u16*)&block->radius;
        prim->y3      = xy;
        prim->y2      = xy;
        ds            = &gDisplayState;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x18;
}
