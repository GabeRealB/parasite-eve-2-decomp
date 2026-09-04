#include "common.h"

#include "main/display.h"

/// Append a 15-bit ABR-1 `DR_TPAGE` for VRAM origin (`tpage`, `arg1`) to OT
/// slot 8.
void Room_Draw42(s32 tpage, s16 arg1)
{
    DR_TPAGE* p;
    s32       y;

    y              = arg1;
    p              = Gpu_PrimCursor;
    Gpu_PrimCursor = p + 1;
    setDrawTPage(p, 1, 0, getTPage(2, 1, tpage & 0x3C0, y));
    addPrim(Gpu_CurrentOt + 8, p);
}
