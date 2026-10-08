/* Part of the factory lift library; see factory_lift.h. */

/// Dispatches the operator panel's current interaction state.
///
/// Requires a live task with state in 0..FACTORY_PANEL_STATE_COUNT-1. Copies
/// the seven callbacks onto the stack and indexes without a bounds check.
/// Initialization owns the panel work; later handlers require that work and
/// may close the session and destroy the task.
static void _factoryPanelRun(Task* task)
{
    TaskFuncTable7 handlers;

    handlers = _gFactoryPanelStates;
    handlers.funcs[task->state](task);
}
