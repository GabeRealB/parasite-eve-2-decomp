#include "common.h"

#include "main/sound.h"
#include "main/task.h"

/// Plays the sound event passed in `spawnArg2` on frames 0 and 0x50 of the
/// task's life and kills the task at frame 0x78.
void func_acropolis_fire_escape_8017FD08(Task* task)
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
