/* Part of the factory lift library; see factory_lift.h. */

void factoryHatchRun(Task* task)
{
    TaskFuncTable3 states;

    states = _gFactoryHatchTaskStates;
    states.funcs[task->state](task);
}
