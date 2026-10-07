/* Part of the action prompt library; see action_prompt.h. */

#ifndef ACTION_PROMPT_RESET_HELPERS_DEFINED
#define ACTION_PROMPT_RESET_HELPERS_DEFINED

/// Restores one port's cursor motion and double-press timers for a new prompt.
static inline void _actionPromptResetPort(ActionPrompt* prompt)
{
    prompt->fixedX                          = 0;
    prompt->fixedY                          = 0;
    prompt->cursorSpeed                     = ACTION_PROMPT_SPEED_RESET;
    prompt->doublePressWindow               = ACTION_PROMPT_DOUBLE_PRESS_FRAMES;
    prompt->buttons.slots[0].framesSinceArm = 0;
    prompt->buttons.slots[1].framesSinceArm = 0;
    prompt->mode                            = ACTION_PROMPT_MODE_IDLE;
}
#endif

/// Resets both ports' cursor motion and double-press timers, then advances the task.
///
/// Borrows the two gameplay-owned prompts regardless of the task's port selector.
/// Positions return to the origin in 1/512-pixel units, motion speed to
/// `ACTION_PROMPT_SPEED_RESET`, the press window to
/// `ACTION_PROMPT_DOUBLE_PRESS_FRAMES` nominal 60-Hz ticks and the cursor to idle.
/// Pixel positions, button classifications and latched press positions are
/// retained until the cursor-motion state updates them. The caller supplies a
/// live task in the reset state; no task work is required or allocated.
void ACTION_PROMPT_RESET_TASK(Task* task)
{
    ActionPrompt* prompt = D_80114D28;
    s32           port;

    for (port = 0; port < ARRAY_SIZE(D_80114D28); port++, prompt++) {
        _actionPromptResetPort(prompt);
    }
    task->state = task->state + 1;
}
