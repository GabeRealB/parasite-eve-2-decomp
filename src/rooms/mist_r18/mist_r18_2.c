#include "common.h"

#include "main/display.h"
#include "rooms/room_common.h"

/// Redraw the room's two backdrop halves as semi-transparent `SPRT`s in OT
/// slot 8, tinting both with `shade`, then append each half's tpage.
void func_mist_r18_8017E144(s16 shade)
{
    SPRT* p;

    p              = (SPRT*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
    setSprt(p);
    setSemiTrans(p, 1);
    p->r0   = shade;
    p->g0   = shade;
    p->b0   = shade;
    p->u0   = 0;
    p->v0   = 0;
    p->x0   = -0xA0;
    p->y0   = -0x78;
    p->clut = 0;
    p->w    = 0xC0;
    p->h    = 0xF0;
    addPrim(Gpu_CurrentOt + 8, p);
    Room_Draw42(0x340, 0);

    p              = (SPRT*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
    setSprt(p);
    setSemiTrans(p, 1);
    p->r0   = shade;
    p->g0   = shade;
    p->b0   = shade;
    p->u0   = 0;
    p->v0   = 0;
    p->x0   = 0x20;
    p->y0   = -0x78;
    p->clut = 0;
    p->w    = 0x80;
    p->h    = 0xF0;
    addPrim(Gpu_CurrentOt + 8, p);
    Room_Draw42(0x280, 0x100);
}

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18_2", func_mist_r18_8017E2C8);
