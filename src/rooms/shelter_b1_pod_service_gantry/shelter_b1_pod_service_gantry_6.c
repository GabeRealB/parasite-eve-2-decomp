#include "common.h"

#include "main/task.h"
#include "rooms/shelter_b1_pod_service_gantry.h"

/// The room task: copies its three-state table
/// `D_shelter_b1_pod_service_gantry_8017D5C4` onto the stack and calls the
/// entry for the task's current state.
void func_shelter_b1_pod_service_gantry_8017D89C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_pod_service_gantry_8017D5C4;
    sp.funcs[task->state](task);
}
