#include "common.h"

#include "main/sound.h"
#include "main/task.h"

#include "rooms/rooms_shared_80182628.h"

void RoomsShared80182628(Task* task)
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
