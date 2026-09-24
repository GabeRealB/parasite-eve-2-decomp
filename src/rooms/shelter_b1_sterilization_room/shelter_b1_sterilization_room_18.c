#include "common.h"

#include "main/display.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b1_sterilization_room.h"

/// Draws the room's two backdrop halves as opaque `SPRT`s in OT slot 8, tinted
/// by `shade`. The source rows, both the sprites' `v` and the tpage row, follow
/// the display buffer being drawn.
void func_shelter_b1_sterilization_room_80180A2C(s32 shade)
{
    SPRT* p;
    s16   tpageY;
    u8    u;
    u8    v;

    if (gDisplayState.drawBuffer == 0) {
        tpageY = 0;
        u      = 0;
        v      = 0;
    } else {
        tpageY = 0x100;
        u      = 0;
        v      = 0x10;
    }

    p              = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
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
    addPrim(gGpuCurrentOt + 8, p);
    func_shelter_b1_sterilization_room_80181308(0, tpageY);

    p              = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
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
    addPrim(gGpuCurrentOt + 8, p);
    func_shelter_b1_sterilization_room_80181308(0xC0, tpageY);
}
