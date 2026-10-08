/* Part of the factory lift library; see factory_lift.h. */

/// Runs the operator panel's default action cursors.
///
/// Requires state 0 for resetting both prompt slots or 1 for their per-frame
/// movement and drawing. Reset advances the task to state 1. The panel owns
/// this child task and kills it during exit; the polling owner releases work.
/// Movement selects port 0 for spawnArg1.value 1, port 1 for 2, and both for
/// other values; the panel spawns it with 1. No task work is allocated here.
static void _factoryPromptTask(Task* task)
{
    TaskFunc states[] = { _actionPromptResetDefault, _actionPromptMoveCursorsDefault };

    states[task->state](task);
}
