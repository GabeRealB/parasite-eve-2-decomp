/* Part of the factory lift library; see factory_lift.h. */

/// Opens the confirmed operator-panel hotspot's command prompt.
///
/// Requires initialized panel work and a live action cursor with a latched
/// Examine/Push choice. Hides and stops that cursor, opens the commands at its
/// screen-pixel position, and advances to the prompt state.
static void _factoryPanelOpenPrompt(Task* task)
{
    ActionPrompt*     prompt = D_80114D28;
    FactoryPanelWork* work   = task->work;

    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    itemMenuOpenHotspotCommands(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = FACTORY_PANEL_STATE_PROMPT;
}
