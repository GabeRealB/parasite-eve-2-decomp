/* Part of the factory lift library; see factory_lift.h. */

void FACTORY_ROOM_INSTANCE_ENTRY_TASK(Task* task)
{
    TaskFuncTable3 stateHandlers = _gFactoryEntryStates;

    stateHandlers.funcs[task->state](task);
}
