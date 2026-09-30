/* Part of the factory lift library; see factory_lift.h. */

/// Runs the room entry task's current state, through a copy of its handler
/// table on the stack.
void factoryEntryTask(Task* task)
{
    TaskFuncTable3 sp;

    sp = _gFactoryEntryStates;
    sp.funcs[task->state](task);
}
