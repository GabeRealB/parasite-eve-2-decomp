/* Part of the factory lift library; see factory_lift.h. */

/// Script state: drops the prompt's highlight and spawns the action prompt at
/// the cursor position with the confirmed hotspot's Examine/Push action, then
/// moves the script to state 4.
void factoryPanelOpenPrompt(Task* task)
{
    ActionPrompt*     prompt = D_80114D28;
    FactoryPanelWork* work   = task->work;

    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    itemMenuOpenHotspotCommands(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = 4;
}
