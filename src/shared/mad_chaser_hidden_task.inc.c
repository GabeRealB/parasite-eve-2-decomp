/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Task callback of the Mad Chaser spawned hidden: runs the handler for
/// `Task::state` from the ten-entry table that adds the emerge, vanish and two
/// death phases.
void madChaserHiddenTask(Task* arg0)
{
    TaskFuncTable10 sp;

    sp = gMadChaserHiddenTaskStates;
    sp.funcs[arg0->state](arg0);
}
