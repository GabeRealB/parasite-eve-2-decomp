#include "common.h"

#include "main/display.h"
#include "main/task.h"
#include "rooms/mist_r18.h"

/// Fade the room back out eight levels a frame, driving both backdrop redraws
/// with complementary shades, and advance the task's state once the level
/// bottoms out. `Task::killCountdown` holds the level.
void func_mist_r18_8017E92C(Task* task)
{
    u16 fade;

    fade                = (u16)task->killCountdown - 8;
    task->killCountdown = fade;
    if ((s16)fade <= 0) {
        task->killCountdown = 0;
        task->state++;
    }
    func_mist_r18_8017E144(task->killCountdown);
    func_mist_r18_8017DF80(0x80 - task->killCountdown);
}

/// Append a 15-bit, ABR-1 `DR_TPAGE` for VRAM origin (`tpage`, `arg1`) to OT
/// slot 8.
void func_mist_r18_8017E994(s32 tpage, s16 arg1)
{
    DR_TPAGE* p;
    s32       y;

    y              = arg1;
    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setDrawTPage(p, 1, 0, getTPage(2, 1, tpage & 0x3C0, y));
    addPrim(gGpuCurrentOt + 8, p);
}
