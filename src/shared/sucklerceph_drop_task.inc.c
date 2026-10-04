/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Task handler of the dropping first enemy: runs the entry of
/// `gSucklercephDropTaskStates` for the task's state with the enemy and the task, from
/// a copy of the table on the stack.
void sucklercephDropTask(Task* arg0)
{
    EnemyTaskFuncTable4 sp;

    sp = gSucklercephDropTaskStates;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}
