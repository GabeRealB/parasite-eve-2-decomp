#include "common.h"

#include "main/task.h"
#include "rooms/shelter_1f_parking_garage.h"

/// The room's controller task: copies its three-entry state table to the
/// stack and runs the entry for the current state.
void func_shelter_1f_parking_garage_8017DF14(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_1f_parking_garage_8017D6A0;
    sp.funcs[task->state](task);
}
