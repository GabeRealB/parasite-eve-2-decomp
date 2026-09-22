#include "common.h"
#include "main/task.h"
#include "rooms/rooms_shared_8017e92c.h"
#include "main/display.h"

void RoomsShared8017df80(s32 shade);

/// Fade the room back out.
void RoomsShared8017e92c(Task* task)
{
    u16 fade;

    fade                = (u16)task->killCountdown - 8;
    task->killCountdown = fade;
    if ((s16)fade <= 0) {
        task->killCountdown = 0;
        task->state++;
    }
    RoomsShared8017e144(task->killCountdown);
    RoomsShared8017df80(0x80 - task->killCountdown);
}

/// Append a 15-bit ABR-1 `DR_TPAGE` for VRAM origin (`tpage`, `arg1`) to OT
/// slot 8.
void Room_Draw42(s32 tpage, s16 arg1)
{
    DR_TPAGE* p;
    s32       y;

    y              = arg1;
    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setDrawTPage(p, 1, 0, getTPage(2, 1, tpage & 0x3C0, y));
    addPrim(gGpuCurrentOt + 8, p);
}
