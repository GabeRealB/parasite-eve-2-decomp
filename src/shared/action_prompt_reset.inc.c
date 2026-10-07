/* Part of the action prompt library; see action_prompt.h. */

#ifndef ACTION_PROMPT_RESET_HELPERS_DEFINED
#define ACTION_PROMPT_RESET_HELPERS_DEFINED

/// Prepares one port's action cursor and press timers for a new prompt.
///
/// Borrows a non-NULL, writable `prompt` for this call. Sets its cursor to the
/// screen center in 1/512-pixel units, its motion multiplier to
/// `ACTION_PROMPT_SPEED_RESET`, its second-press window to
/// `ACTION_PROMPT_DOUBLE_PRESS_FRAMES` nominal 60-Hz ticks and its sprite to idle.
/// Both confirm and cancel arm counters start at zero. The published pixel
/// position, button classifications and latched press positions are retained.
/// The cursor-motion update classifies presses against the retained positions
/// before publishing the new pixel position.
static inline void _actionPromptResetPort(ActionPrompt* prompt)
{
    enum {
        ACTION_PROMPT_CONFIRM_SLOT = 0,
        ACTION_PROMPT_CANCEL_SLOT  = 1
    };

    prompt->fixedX                                                   = 0;
    prompt->fixedY                                                   = 0;
    prompt->cursorSpeed                                              = ACTION_PROMPT_SPEED_RESET;
    prompt->doublePressWindow                                        = ACTION_PROMPT_DOUBLE_PRESS_FRAMES;
    prompt->buttons.slots[ACTION_PROMPT_CONFIRM_SLOT].framesSinceArm = 0;
    prompt->buttons.slots[ACTION_PROMPT_CANCEL_SLOT].framesSinceArm  = 0;
    prompt->mode                                                     = ACTION_PROMPT_MODE_IDLE;
}
#endif

/// Prepares both ports' action cursors and advances the task to cursor motion.
///
/// Requires a live, non-NULL task and the two writable gameplay-owned prompts.
/// Resets both prompts regardless of `task->spawnArg1.value`: centers their
/// 1/512-pixel positions, selects `ACTION_PROMPT_SPEED_RESET` and the idle
/// sprite, sets a 15-tick double-press window and clears both button arm counters.
/// Ticks are nominal 60-Hz units, advanced by cursor motion's `frameTicks`.
/// Published pixel positions, button classifications and latched press positions
/// are preserved; motion classifies the next presses using that retained data
/// before publishing the centered position plus any new movement.
///
/// Increments `Task::state` by one. Prompt dispatchers call this in state 0 and
/// move the selected port(s) in state 1. No task work is required or allocated.
static void ACTION_PROMPT_RESET_TASK(Task* task)
{
    ActionPrompt* prompt = D_80114D28;
    s32           port;

    for (port = 0; port < ARRAY_SIZE(D_80114D28); port++, prompt++) {
        _actionPromptResetPort(prompt);
    }
    task->state = task->state + 1;
}
