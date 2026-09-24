#include "common.h"

#include "main/sound.h"
#include "main/task.h"

/// A one-shot sound cue on a timer: fires its spawn argument as a sound event
/// on the first frame, repeats it at frame 0x50 and kills itself at 0x78.
void func_shelter_b1_underground_parking_801831F4(Task* task)
{
    s32 zero;

    switch (task->state) {
        case 0x50:
        case 0x0:
            zero = 0;
            TOUCH_REG(zero);
            SndEvt_EnqueueType6((s32)task->spawnArg2, zero, zero);
            break;
        case 0x78:
            Task_RequestKill(task, 0);
            return;
    }
    task->state += 1;
}
