#include "common.h"

#include "main/display.h"
#include "rooms/room_common.h"

/// Redraw the room's two backdrop halves as opaque `SPRT`s in OT slot 8,
/// tinting both with `shade`. Which display buffer is live shifts the source
/// rows in the off-screen staging area, so both the sprites' `v` texcoord and
/// the tpage row handed to `Room_Draw42` move with it.
void RoomsShared8017df80(s32 shade)
{
    SPRT* p;
    s16   tpageY;
    u8    u;
    u8    v;

    if (Display_State.field_1f == 0) {
        tpageY = 0;
        u      = 0;
        v      = 0;
    } else {
        tpageY = 0x100;
        u      = 0;
        v      = 0x10;
    }

    p              = (SPRT*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
    setSprt(p);
    p->r0   = shade;
    p->g0   = shade;
    p->b0   = shade;
    p->u0   = u;
    p->v0   = v;
    p->x0   = -0xA0;
    p->y0   = -0x78;
    p->clut = 0;
    p->w    = 0xC0;
    p->h    = 0xF0;
    addPrim(Gpu_CurrentOt + 8, p);
    Room_Draw42(0, tpageY);

    p              = (SPRT*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
    setSprt(p);
    p->r0   = shade;
    p->g0   = shade;
    p->b0   = shade;
    p->x0   = 0x20;
    p->u0   = u;
    p->v0   = v;
    p->y0   = -0x78;
    p->clut = 0;
    p->w    = 0x80;
    p->h    = 0xF0;
    addPrim(Gpu_CurrentOt + 8, p);
    Room_Draw42(0xC0, tpageY);
}
