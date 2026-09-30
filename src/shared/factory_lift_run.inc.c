/* Part of the factory lift library; see factory_lift.h. */

/// Runs the factory model task's current state, through a copy of its handler
/// table on the stack.
void factoryLiftRun(Task* task)
{
    TaskFuncTable3 sp;

    sp = _gFactoryLiftStates;
    sp.funcs[task->state](task);
}
