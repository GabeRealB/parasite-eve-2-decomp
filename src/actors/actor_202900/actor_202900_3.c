#include "common.h"

#include "actors/actor_202900.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// Update of the actor's task: publishes the task's work block in
/// `D_actor_202900_80156E54`, so the overlay's other functions can reach it
/// without the task in hand, then dispatches on the task's state to the setup
/// handler `func_actor_202900_80149E24` (state 0) or the per-frame handler
/// `func_actor_202900_8014A0B4` (state 1), passing the task's enemy record
/// along with the task.
void func_actor_202900_8014A02C(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_202900_80149E24,
        func_actor_202900_8014A0B4,
    };

    D_actor_202900_80156E54 = (Actor202900Work*)task->work;
    fns[task->state](task->spawnArg2, task);
}
