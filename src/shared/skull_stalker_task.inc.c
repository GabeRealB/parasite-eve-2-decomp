/* Part of the Skull Stalker library; see skull_stalker.h. */

/// Task handler of the second enemy: runs the entry of `gSkullStalkerTaskStates` for
/// the task's state with the enemy and the task, from a copy of the table on
/// the stack.
void skullStalkerTask(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = gSkullStalkerTaskStates;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}
