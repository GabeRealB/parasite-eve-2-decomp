/* Part of the action prompt library; see action_prompt.h. */

/// State 0 of the prompt script task: resets both action-prompt slots. The
/// fixed-point cursor returns to the origin, the speed to
/// `ACTION_PROMPT_SPEED_RESET`, the double-press window to
/// `ACTION_PROMPT_DOUBLE_PRESS_FRAMES` and the sprite to the idle cursor.
/// The task then advances one state.
void actionPromptReset(Task* task)
{
    ActionPrompt* prompt = D_80114D28;
    s32           i;

    for (i = 0; i < 2; i++, prompt++) {
        prompt->fixedX                      = 0;
        prompt->fixedY                      = 0;
        prompt->cursorSpeed                 = ACTION_PROMPT_SPEED_RESET;
        prompt->doublePressWindow           = ACTION_PROMPT_DOUBLE_PRESS_FRAMES;
        prompt->buttons.slots[0].heldFrames = 0;
        prompt->buttons.slots[1].heldFrames = 0;
        prompt->mode                        = ACTION_PROMPT_MODE_IDLE;
    }
    task->state = task->state + 1;
}
