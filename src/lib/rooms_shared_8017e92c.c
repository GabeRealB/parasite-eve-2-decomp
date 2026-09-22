#include "common.h"

#include "main/task.h"

#include "rooms/rooms_shared_8017e92c.h"

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
