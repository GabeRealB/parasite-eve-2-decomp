/* Part of the factory lift library; see factory_lift.h. */

/// Script state: arms the action prompt at `ACTION_PROMPT_SPEED_AIM` with the
/// idle cursor, clears its screen position and steps the script on one state.
void factoryPanelArmPrompt(Task* task)
{
    ActionPrompt* prompt = D_80114D28;

    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    prompt->mode        = ACTION_PROMPT_MODE_IDLE;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}
