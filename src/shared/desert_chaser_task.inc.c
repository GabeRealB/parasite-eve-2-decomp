/* Part of the Desert Chaser library; see desert_chaser.h. */

/// The enemy task's per-frame entry: runs the handler for the task's current
/// state - spawn, tick or teardown - from a stack copy of the state table.
void desertChaserTask(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = gDesertChaserTaskStates;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}
