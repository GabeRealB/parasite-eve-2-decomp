/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Dispatches the hidden/drop Sucklerceph task through its four enemy states.
///
/// Requires a live enemy in `spawnArg2.pointer` and state 0..3: hidden setup,
/// ordinary update, death/teardown, or dropping into place. Copies the carrier's
/// four callbacks to the stack and indexes without checking. A callback may
/// destroy the enemy and task; no owned data is accessed after dispatch.
static void _sucklercephDropTask(Task* task)
{
    EnemyTaskFuncTable4 states;

    states = gSucklercephDropTaskStates;
    states.funcs[task->state](task->spawnArg2.pointer, task);
}
