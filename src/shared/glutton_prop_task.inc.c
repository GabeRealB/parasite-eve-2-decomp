/* Part of the Glutton library; see glutton.h. */

/// Runs the lifecycle of a Glutton escort model attached to the host.
///
/// Requires a live TMD task with its owned `Enemy` in `spawnArg2.pointer`.
/// `Task::state` is an unchecked index: 0 applies the host's placement texture
/// offsets and advances to 1, 1 refreshes the composed root coordinate, and
/// 2 releases the enemy and starts task teardown. Setup requires a live parent
/// enemy whose placement exists in the synchronized current area variant.
/// Dispatch may release both task and enemy, so neither is accessed afterwards.
void GLUTTON_PROP_TASK(Task* task)
{
    EnemyTaskFuncTable3 stateHandlers = gGluttonPropStates;

    stateHandlers.funcs[task->state](task->spawnArg2.pointer, task);
}
