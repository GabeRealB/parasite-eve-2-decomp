#include "common.h"

#include "main/task.h"
#include "rooms/shelter_b1_golem_freezer_1.h"

/// Runs one tick of the room task through the three-state table
/// `D_shelter_b1_golem_freezer_1_8017D5C4`, copying the table onto the stack
/// and calling the entry for the task's current state.
void func_shelter_b1_golem_freezer_1_8017D6EC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_golem_freezer_1_8017D5C4;
    sp.funcs[task->state](task);
}
