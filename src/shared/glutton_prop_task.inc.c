/* Part of the Glutton library; see glutton.h. */

/// Task body of the escort models: runs the task state's handler from a stack
/// copy of `gGluttonPropStates`. Each package carries two identical copies, one
/// for escorts 0-5 and one for escort 6 (`func_actor_403200_8014148C` /
/// `func_actor_444000_8014382C`).
void gluttonPropTask(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = gGluttonPropStates;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}
