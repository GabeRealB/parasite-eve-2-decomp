/* Part of the factory lift library; see factory_lift.h. */

/// Runs the current state of the room's cutscene sequence, copying the room's
/// three handlers onto the stack first so the call goes through a local table
/// rather than through `.rodata`. A handler returning non-zero has finished its
/// part of the scene, which drops the sequence back to the shared state 0.
void factoryHatchUpdate(Task* task)
{
    FactoryHatchWork*          work = task->work;
    FactoryHatchStateFuncTable states;

    states = _gFactoryHatchStates;
    if (states.funcs[work->state](task) != 0) {
        work->state = FACTORY_HATCH_STATE_WATCH;
    }
}
