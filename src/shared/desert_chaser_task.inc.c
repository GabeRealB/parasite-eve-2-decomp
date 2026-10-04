/* Part of the Desert Chaser library; see desert_chaser.h. */

/// The enemy task's per-frame entry: runs the handler for the task's current
/// state from a stack copy of the state table. The states are spawn, tick and
/// teardown; the regular build runs a fourth between tick and teardown, which
/// settles a pending release before advancing.
void desertChaserTask(Task* task)
{
    DesertChaserTaskStates states;

    states = gDesertChaserTaskStates;
    states.funcs[task->state](task->spawnArg2.pointer, task);
}
