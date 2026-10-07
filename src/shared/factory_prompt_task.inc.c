/* Part of the factory lift library; see factory_lift.h. */

/// The prompt task the script spawns: resets both action-prompt slots, then
/// moves and draws the cursors every frame.
void factoryPromptTask(Task* task)
{
    TaskFunc states[2] = { actionPromptReset, _actionPromptMoveCursorsDefault };

    states[task->state](task);
}
