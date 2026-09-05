#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/4CC.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017f10c.h"

/// `rtps` / `rtpt` / `mvmva`. The `inline_c.h` macros of those names assemble
/// to different words, so spell the instructions out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

/// Draws one mote of a room's ambient effect: the unit quad
/// `D_80111E38` scaled to `arg1` half-size, rotated by the mote's own
/// `GsCOORDINATE2` and then projected through `GsWSMATRIX` into a 0x24-byte
/// `G_SCRATCH_HEAD` block. The first corner goes through `rtps` and the other
/// three through `rtpt`; motes inside `otz` 0x11 are dropped.
///
/// `arg2` is the fade level: zero draws the mote as a raw texture
/// (`setShadeTex`), otherwise it is the grey the quad is modulated by and the
/// quad is drawn semi-transparent, which is how the task's fade-out step
/// (`RoomsShared8017f10c` state 3) dims it away.
void RoomsShared8017f10cSub(GsCOORDINATE2* arg0, s32 arg1, s16 arg2)
{
    register GsCOORDINATE2* coord asm("t7");
    register void**         scratch asm("a0");
    u8*                     head;
    RoomQuadScratch*        blk;
    POLY_FT4*               prim;
    GpQuadCorner*           tbl;
    SVECTOR*                sv;
    MATRIX*                 wm;
    s32                     i;

    coord   = arg0;
    scratch = (void**)G_SCRATCH_HEAD;
    i       = 0;
    wm      = &coord->workm;
    tbl     = D_80111E38;
    head    = (u8*)*scratch - sizeof(RoomQuadScratch);
    /* `head` and `blk` have to stay separate registers: the ROM computes the
       block address into a scratch register and copies it into the one the
       rest of the function uses. */
    SOFT_TOUCH_REG(head);
    blk      = (RoomQuadScratch*)head;
    *scratch = blk;
    do {
        blk->v[i].vx = tbl[i].x * arg1;
        // Spelled as an offset rather than `&blk->v[i]` so it stays a separate
        // pointer from the one the GTE macros below take; writing both the same
        // way lets CSE fold them into one register and the loop stops matching.
        sv     = (SVECTOR*)((u8*)blk + i * sizeof(SVECTOR) + OFFSET_OF(RoomQuadScratch, v));
        sv->vy = 0;
        sv->vz = tbl[i].y * arg1;
        gte_SetRotMatrix(wm);
        gte_ldv0(&blk->v[i]);
        gte_rtv0_real();
        gte_stsv(&blk->v[i]);
        *(u16*)&blk->v[i].vx = *(u16*)&blk->v[i].vx + *(u16*)&coord->workm.t[0];
        *(u16*)&sv->vy       = *(u16*)&sv->vy + *(u16*)&coord->workm.t[1];
        i++;
        *(u16*)&sv->vz = *(u16*)&sv->vz + *(u16*)&coord->workm.t[2];
    } while (i < 4);

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->v[0]);
    gte_rtps_real();
    prim           = (POLY_FT4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&prim->x0);
    gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
    gte_rtpt_real();
    setUV4(prim, 0, 0xE8, 7, 0xE8, 0, 0xEF, 7, 0xEF);
    gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
    gte_stszotz(&blk->otz);
    if (blk->otz >= 0x11) {
        if (arg2 != 0) {
            setRGB0(prim, arg2, arg2, arg2);
            setSemiTrans(prim, 1);
        } else {
            setShadeTex(prim, 1);
        }
        prim->tpage = 0x2B;
        prim->clut  = 0x4390;
        addPrim((u_long*)(((((u32)blk->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(RoomQuadScratch);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_roof_garden/acropolis_roof_garden_4", func_acropolis_roof_garden_8017F870);

INCLUDE_ASM("rooms/nonmatchings/acropolis_roof_garden/acropolis_roof_garden_4", func_acropolis_roof_garden_8017FA14);

/// Item-pickup model task step: the item's mesh is only visible from views 5
/// through 7, and stays hidden once the item's 2-bit flag reads 2 (already
/// taken). The three hidden cases are written as separate tests so the two view
/// comparisons are not folded into one unsigned range check.
void func_acropolis_roof_garden_80180160(Task* task)
{
    GpItemObj8* obj;
    TmdObject*  tmd;
    s32         flag;
    s32         view;

    obj  = (GpItemObj8*)task->spawnArg2;
    tmd  = (TmdObject*)task->extra;
    flag = Gp_GetCurBit2Flag(obj->field_8);
    view = Gp_GetViewIndex();
    if (view >= 8) {
        tmd->field_C = 0x80;
    } else if (view < 5) {
        tmd->field_C = 0x80;
    } else if (flag == 2) {
        tmd->field_C = 0x80;
    } else {
        tmd->field_C = 8;
        tmd->field_E = 0;
    }
}
