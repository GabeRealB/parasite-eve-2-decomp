/* Part of the Skull Stalker library; see skull_stalker.h. */

/// Dispatches the Skull Stalker's spawn, active or death task state.
///
/// State must be 0..2, with the enemy in spawnArg2 and a live TMD body. Copies
/// the carrier's three callback pointers by value before dispatch; the handler
/// can destroy the task, so no task access follows the call.
static void _skullStalkerTask(Task* task)
{
    EnemyTaskFuncTable3 states;

    states = gSkullStalkerTaskStates;
    states.funcs[task->state](task->spawnArg2.pointer, task);
}
