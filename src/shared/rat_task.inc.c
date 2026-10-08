/* Part of the Rat library; see rat.h. */

/// Dispatches the rat task's spawn, update or death handler.
///
/// Requires an `Enemy` in `Task::spawnArg2.pointer` and a task state in 0..2. Copies the carrier's
/// three-entry callback table by value before dispatch; there is no bounds check.
static void _ratTask(Task* actor)
{
    EnemyTaskFuncTable3 handlers;

    handlers = gRatStateHandlers;
    handlers.funcs[actor->state](actor->spawnArg2.pointer, actor);
}
