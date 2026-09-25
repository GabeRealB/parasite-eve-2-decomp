#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"

#include "rooms/room_common.h"

/// The night motel room's drawable points, one 8-byte `SVECTOR` per disc. Each
/// is reached by its own address, so the compiler materialises it whole into
/// `$a0` rather than indexing one base.
extern SVECTOR D_dryfield_night_motel_room_3_8017DA84;
extern SVECTOR D_dryfield_night_motel_room_3_8017DA8C;
extern SVECTOR D_dryfield_night_motel_room_3_8017DA94;

/// Queues a flickering disc at the world point `arg0`: a semi-transparent
/// `POLY_FT4` square centred on the point's projection, with half-width
/// `arg2 * 39 / otz`, textured from the 40-texel cell `arg1` of tpage 0x2B
/// and shaded 0x20 or 0x30 on alternate frames. Points closer than OTZ 0x11
/// are skipped.
void func_dryfield_night_motel_room_3_8017D738(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    u8*                tmp;
    RoomDraw25Scratch* block;
    POLY_FT4*          prim;
    DisplayState*      ds;
    s32                tex;
    s32                idx;
    s32                u0;
    s32                u1;
    register s32       sarg asm("v1");
    s32                t;
    s32                field8;
    s32                blend;
    s32                v;
    u8                 code;
    s16                xy;

    tex = arg1;
    CLOBBER_REG(a1);
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    tmp     = head - 0xC;
    block   = (RoomDraw25Scratch*)tmp;
    SOFT_TOUCH_REG(block);
    *scratch = tmp;

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&((RoomDraw25Scratch*)(head - 0xC))->sx);
    gte_stszotz(&block->otz);
    if (((RoomDraw25Scratch*)(head - 0xC))->otz >= 0x11) {
        ds          = &gDisplayState;
        idx         = (s16)tex;
        field8      = (u8)ds->animFrame;
        prim->tpage = 0x2B;
        prim->clut  = (idx & 0x3F) | 0x4380;
        u0          = idx * 40;
        u1          = u0 + 0x27;
        prim->u0    = u0;
        prim->u2    = u0;
        SOFT_USE_REG(u0);
        v        = 0x27;
        prim->u1 = u1;
        prim->u3 = u1;
        SOFT_USE_REG(u1);
        sarg     = arg2 << 16;
        prim->v2 = v;
        prim->v3 = v;
        SCHED_BARRIER();
        code     = prim->code;
        sarg     = sarg >> 16;
        prim->v0 = 0;
        prim->v1 = 0;
        blend    = ((field8 & 1) * 16) + 0x20;
        COMPILER_BARRIER();
        prim->code = code | 2;
        t          = sarg * 40;
        setRGB0(prim, blend, blend, blend);
        ((RoomDraw25Scratch*)tmp)->radius =
            (t - sarg) / ((RoomDraw25Scratch*)(head - 0xC))->otz;
        xy       = ((RoomDraw25Scratch*)tmp)->sx - (u16)((RoomDraw25Scratch*)tmp)->radius;
        prim->x2 = xy;
        prim->x0 = xy;
        xy       = ((RoomDraw25Scratch*)tmp)->sx + (u16)((RoomDraw25Scratch*)tmp)->radius;
        prim->x3 = xy;
        prim->x1 = xy;
        xy       = ((RoomDraw25Scratch*)tmp)->sy - (u16)((RoomDraw25Scratch*)tmp)->radius;
        prim->y1 = xy;
        prim->y0 = xy;
        xy       = ((RoomDraw25Scratch*)tmp)->sy + (u16)((RoomDraw25Scratch*)tmp)->radius;
        prim->y3 = xy;
        prim->y2 = xy;
        addPrim((u_long*)(((((u32)((RoomDraw25Scratch*)(head - 0xC))->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0xC);
}

/// Per-frame effect: queues the room's glowing discs for the visit
/// `gGameSession->at4.loc.view` selects - visit 3 the second point, visit 4 the
/// second point then the first, visits 10 and 11 the first alone, visit 8 the
/// third with a wider UV column and half-extent. Visits outside those draw
/// nothing.
void func_dryfield_night_motel_room_3_8017D9B4(void)
{
    switch (gGameSession->at4.loc.view) {
        case 3:
            func_dryfield_night_motel_room_3_8017D738(&D_dryfield_night_motel_room_3_8017DA8C, 1, 0x240);
            break;
        case 4:
            func_dryfield_night_motel_room_3_8017D738(&D_dryfield_night_motel_room_3_8017DA8C, 1, 0x240);
            /* fallthrough */
        case 10:
        case 11:
            func_dryfield_night_motel_room_3_8017D738(&D_dryfield_night_motel_room_3_8017DA84, 1, 0x200);
            break;
        case 8:
            func_dryfield_night_motel_room_3_8017D738(&D_dryfield_night_motel_room_3_8017DA94, 2, 0x180);
            break;
    }
}
