/* Part of the Moth library; see moth.h. */

/// Dispatches the moth's spawn, living-update or death handler.
///
/// Task state must be 0..2 and spawnArg2 must point to its live Enemy. Copies
/// the three-handler table by value before invoking the selected callback;
/// spawn acquires work and the death handler eventually destroys the task.
static void _mothTask(Task* task)
{
    EnemyTaskFuncTable3 handlers;

    handlers = gMothStateHandlers;
    handlers.funcs[task->state](task->spawnArg2.pointer, task);
}
