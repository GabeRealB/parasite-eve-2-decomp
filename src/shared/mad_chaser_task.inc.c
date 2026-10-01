/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Runs the handler for the task's `Task::state` from the six-entry table.
void madChaserTask(Task* arg0)
{
    TaskFuncTable6 sp;

    sp = gMadChaserTaskStates;
    sp.funcs[arg0->state](arg0);
}
