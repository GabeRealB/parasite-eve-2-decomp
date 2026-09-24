#include "common.h"

#include "main/sound.h"
#include "main/task.h"

#include "rooms/dryfield_gas_station.h"

/// Task body that enqueues the type-6 sound event held in `spawnArg2` on its
/// first tick and again at tick 0x50, and kills itself at tick 0x78; `state`
/// counts the ticks.
void func_dryfield_gas_station_8017FCBC(Task* task)
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
