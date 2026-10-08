/* Part of the factory lift library; see factory_lift.h. */

void factoryLiftRun(Task* task)
{
    TaskFuncTable3 states;

    states = _gFactoryLiftStates;
    states.funcs[task->state](task);
}
