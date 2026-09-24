#include "common.h"

#include "main/sound.h"
#include "main/task.h"

/// Sound task: plays the sound event `spawnArg2` on its first tick and again on
/// tick 0x50, and asks to be killed on tick 0x78.
void func_shelter_b6_nursery_8017FD3C(Task* task)
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
