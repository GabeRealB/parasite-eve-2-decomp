/* Part of the factory lift library; see factory_lift.h. */

/// Dispatches the hatch's watch, open or close state and returns settled swings to watching.
///
/// Requires a live hatch model and `FactoryHatchWork` whose selector names a
/// `FACTORY_HATCH_STATE_` handler. Copies the complete handler table to the
/// stack, then calls the selected slot without a range check. A zero result
/// preserves the handler's selected state; nonzero selects WATCH next frame.
static void _factoryHatchUpdate(Task* task)
{
    FactoryHatchWork*          work = task->work;
    FactoryHatchStateFuncTable states;

    states = _gFactoryHatchStates;
    if (states.funcs[work->state](task) != 0) {
        work->state = FACTORY_HATCH_STATE_WATCH;
    }
}
