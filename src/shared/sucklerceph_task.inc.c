/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Dispatches the standing Sucklerceph's spawn, active or death task state.
///
/// `task->state` must be in 0..2, and `spawnArg2.pointer` must hold its owning
/// enemy. Copies the carrier's three callbacks before dispatch; there is no
/// bounds check. Spawn or death may destroy either argument during the call.
static void _sucklercephTask(Task* task)
{
    EnemyTaskFuncTable3 states;

    states = gSucklercephTaskStates;
    states.funcs[task->state](task->spawnArg2.pointer, task);
}
