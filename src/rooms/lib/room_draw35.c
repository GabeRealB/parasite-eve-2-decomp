#include "common.h"

#include "main/display.h"
#include "main/mem.h"
#include "rooms/room_common.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

/// `rtps` / `mvmva` on v0. The `inline_c.h` macros of those names assemble to
/// different words, so spell the instructions out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

/// Queues one screen-aligned textured sprite. `arg1` is rotated out of `arg0`'s
/// local space by the coordinate's `workm` and offset by its translation, then
/// projected through `GsWSMATRIX` with a single `RTPS` into a 0x14-byte
/// `G_SCRATCH_HEAD` block; anything nearer than `otz` 0x11 is dropped
/// entirely.
///
/// The primitive is a semi-transparent `POLY_FT4` on tpage 0x2B. `arg2` picks
/// one of the 40-texel-wide animation frames laid out along the texture page's
/// top row -- u spans `arg2 * 40 .. arg2 * 40 + 39` at v 0..0x27 -- and also
/// selects the clut, `(arg2 & 0x3F) | 0x4380`. `arg3` is a signed half-extent,
/// so the axis-aligned quad is `(s16)arg3 * 39 / otz` in every direction from
/// the projected centre and shrinks with distance. The flat colour alternates
/// between 0x20 and 0x30 on the parity of `Display_State.field_8`, which makes
/// the sprite flicker. Shared body, linked into every room overlay that uses
/// it.
void Room_Draw35(GsCOORDINATE2* arg0, SVECTOR* arg1, s32 arg2, s32 arg3)
{
    void**             scratch;
    u8*                head;
    RoomDraw35Scratch* block;
    POLY_FT4*          prim;
    DisplayState*      ds;
    s32                su;
    s32                sv;
    s32                u0;
    s32                u1;
    s32                flip;
    s32                rgb;
    s16                xy;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    *scratch = head - 0x14;
    block    = (RoomDraw35Scratch*)(head - 0x14);

    gte_SetRotMatrix(&arg0->workm);
    gte_ldv0(arg1);
    gte_rtv0_real();
    gte_stsv(&((RoomDraw35Scratch*)(head - 0x14))->vec);
    block->vec.vx = *(u16*)&block->vec.vx + *(u16*)&arg0->workm.t[0];
    block->vec.vy = *(u16*)&block->vec.vy + *(u16*)&arg0->workm.t[1];
    block->vec.vz = *(u16*)&block->vec.vz + *(u16*)&arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomDraw35Scratch*)(head - 0x14))->vec);
    gte_rtps_real();

    prim           = (POLY_FT4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&((RoomDraw35Scratch*)(head - 0x14))->sx);
    gte_stszotz(&block->otz);
    if (((RoomDraw35Scratch*)(head - 0x14))->otz >= 0x11) {
        ds          = &Display_State;
        flip        = (u8)ds->field_8;
        su          = (s16)arg2;
        sv          = (s16)arg3;
        prim->tpage = 0x2B;
        prim->clut  = (su & 0x3F) | 0x4380;
        u0          = su * 0x28;
        u1          = u0 + 0x27;
        prim->u1    = u1;
        prim->u3    = u1;
        prim->u0    = u0;
        prim->u2    = u0;
        prim->v0    = 0;
        prim->v1    = 0;
        prim->v2    = 0x27;
        prim->v3    = 0x27;
        rgb         = (flip & 1) << 4;
        rgb        += 0x20;
        setSemiTrans(prim, 1);
        prim->r0         = rgb;
        prim->g0         = rgb;
        prim->b0         = rgb;
        block->halfWidth = (sv * 0x27) / block->otz;
        xy               = *(u16*)&block->sx - *(u16*)&block->halfWidth;
        prim->x2         = xy;
        prim->x0         = xy;
        xy               = *(u16*)&block->sx + *(u16*)&block->halfWidth;
        prim->x3         = xy;
        prim->x1         = xy;
        xy               = *(u16*)&block->sy - *(u16*)&block->halfWidth;
        prim->y1         = xy;
        prim->y0         = xy;
        xy               = *(u16*)&block->sy + *(u16*)&block->halfWidth;
        prim->y3         = xy;
        prim->y2         = xy;
        addPrim((u_long*)(((((u32)block->otz << ds->field_128) >> 2) & 0xFFC) +
                          (s32)Gpu_CurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
}
