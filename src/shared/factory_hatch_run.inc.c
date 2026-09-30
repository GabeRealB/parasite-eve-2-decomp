/* Part of the factory lift library; see factory_lift.h. */

/// Runs the cutscene task's current state, through a copy of its handler table
/// on the stack.
void factoryHatchRun(Task* task)
{
    TaskFuncTable3 sp;

    sp = _gFactoryHatchTaskStates;
    sp.funcs[task->state](task);
}
