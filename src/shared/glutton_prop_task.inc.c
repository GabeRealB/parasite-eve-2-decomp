/* Part of the Glutton library; see glutton.h. */

/// Dispatches a Glutton escort model's setup, coordinate refresh or teardown.
///
/// Requires a live TMD body and its owning `Enemy` in `spawnArg2.pointer`.
/// `state` selects 0 (host-placement textures), 1 (coordinate refresh) or
/// 2 (enemy release and task teardown), without a bounds check. Setup also
/// requires a live parent enemy with a valid placement in the current area
/// variant and advances to state 1. Teardown invalidates the enemy and may
/// release the task; neither is accessed after dispatch.
void GLUTTON_PROP_TASK(Task* task)
{
    EnemyTaskFuncTable3 stateHandlers;

    stateHandlers = gGluttonPropStates;
    stateHandlers.funcs[task->state](task->spawnArg2.pointer, task);
}
