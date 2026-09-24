#include "common.h"

#include "main/display.h"
#include "main/task.h"
#include "rooms/shelter_b1_sterilization_room.h"

/// Steps `killCountdown` down by 8 each frame, advancing the task once it
/// reaches zero, and redraws the backdrop with the semi-transparent copy at
/// that level and the opaque copy at the rest.
void func_shelter_b1_sterilization_room_801812A0(Task* task)
{
    u16 fade;

    fade                = (u16)task->killCountdown - 8;
    task->killCountdown = fade;
    if ((s16)fade <= 0) {
        task->killCountdown = 0;
        task->state++;
    }
    func_shelter_b1_sterilization_room_80180BF0(task->killCountdown);
    func_shelter_b1_sterilization_room_80180A2C(0x80 - task->killCountdown);
}

void func_shelter_b1_sterilization_room_80181308(s32 tpage, s16 arg1)
{
    DR_TPAGE* p;
    s32       y;

    y              = arg1;
    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setDrawTPage(p, 1, 0, getTPage(2, 1, tpage & 0x3C0, y));
    addPrim(gGpuCurrentOt + 8, p);
}
