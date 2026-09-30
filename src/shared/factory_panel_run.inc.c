/* Part of the factory lift library; see factory_lift.h. */

/// Runs the script task's current state. The seven handlers are copied onto
/// the stack first, so the call goes through a local table rather than through
/// `.rodata`.
void factoryPanelRun(Task* task)
{
    TaskFuncTable7 sp;

    sp = _gFactoryPanelStates;
    sp.funcs[task->state](task);
}
