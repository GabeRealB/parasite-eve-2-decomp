#include "common.h"

#include "main/task.h"

extern u8 D_801153F4;

void func_mist_parking_801839CC(Task* task);
void func_mist_parking_80183A28(Task* task);

/// Runs the parking-lot cap cutscene's sub-state handler for `task`, unless the
/// global suspend flag is set.
void func_mist_parking_80183B40(Task* task)
{
    TaskFunc states[3] = { func_mist_parking_801839CC, func_mist_parking_80183A28, Task_Kill };

    if (D_801153F4 == 0) {
        states[task->state](task);
    }
}
