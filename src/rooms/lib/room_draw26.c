#include "common.h"

#include "main/display.h"
#include "rooms/room_common.h"

#include <psyq/libgpu.h>

/// Outlines `rect` on screen in (`r`, `g`, `b`) with four unconnected flat
/// `LINE_F2`s -- top, right, bottom and left edge of the rectangle spanning
/// (`x`, `y`) to (`x + w`, `y + h`) -- each linked into `Gpu_CurrentOt[1]`.
///
/// Shared body, linked into every room overlay that uses it.
void Room_Draw26(RoomRect* rect, u8 r, u8 g, u8 b)
{
    LINE_F2* line;

    line           = (LINE_F2*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(line + 1);
    setLineF2(line);
    line->x0 = rect->x;
    line->y0 = rect->y;
    line->x1 = rect->x + rect->w;
    line->y1 = rect->y;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(Gpu_CurrentOt + 1, line);

    line           = (LINE_F2*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(line + 1);
    setLineF2(line);
    line->x0 = rect->x + rect->w;
    line->y0 = rect->y;
    line->x1 = rect->x + rect->w;
    line->y1 = rect->y + rect->h;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(Gpu_CurrentOt + 1, line);

    line           = (LINE_F2*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(line + 1);
    setLineF2(line);
    line->x0 = rect->x + rect->w;
    line->y0 = rect->y + rect->h;
    line->x1 = rect->x;
    line->y1 = rect->y + rect->h;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(Gpu_CurrentOt + 1, line);

    line           = (LINE_F2*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(line + 1);
    setLineF2(line);
    line->x0 = rect->x;
    line->y0 = rect->y + rect->h;
    line->x1 = rect->x;
    line->y1 = rect->y;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(Gpu_CurrentOt + 1, line);
}
