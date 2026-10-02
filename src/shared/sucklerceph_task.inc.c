/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Task handler of the first enemy: runs the entry of `gSucklercephTaskStates` for
/// the task's state with the enemy and the task, from a copy of the table on
/// the stack.
void sucklercephTask(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = gSucklercephTaskStates;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}
