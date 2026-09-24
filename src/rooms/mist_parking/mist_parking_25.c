#include "common.h"

#include "main/sound.h"
#include "main/task.h"

/// Plays the sound event in `spawnArg2` on its first frame and again at frame
/// 0x50, then asks for the task's own kill at frame 0x78.
void func_mist_parking_80182628(Task* task)
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
