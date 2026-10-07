/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Dispatches one task-state frame of the Mad Chaser form that can emerge.
///
/// `Task::state` must be in 0..9 (`MAD_CHASER_TASK_*`), with that handler's storage
/// initialized and all callbacks loaded. Copies the carrier's ten handlers
/// before calling the selected one once; the handler owns any state changes
/// and teardown. The task descriptor supplies the model and enemy instance.
static void _madChaserHiddenTask(Task* task)
{
    TaskFuncTable10 states;

    states = gMadChaserHiddenTaskStates;
    states.funcs[task->state](task);
}
