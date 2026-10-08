/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Dispatches one task-state frame of the ordinary Mad Chaser form.
///
/// Task::state must be in 0..5 (spawn through despawn), with that handler's
/// storage initialized and callbacks loaded. Copies the carrier's six handlers
/// before calling the selected one once; the handler owns state changes and
/// teardown. The descriptor supplies the live model and enemy spawn argument.
static void _madChaserTask(Task* task)
{
    TaskFuncTable6 states;

    states = gMadChaserTaskStates;
    states.funcs[task->state](task);
}
