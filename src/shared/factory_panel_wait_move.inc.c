/* Part of the factory lift library; see factory_lift.h. */

/// Resumes the operator panel's scan after the lift reports a settled move.
///
/// Requires initialized panel work and a live prompt. Keeps the cursor hidden
/// and stopped until `moveSettled` is nonzero, then consumes that notification,
/// selects the powered or unpowered panel view and starts a ten-frame scan
/// delay. The panel owns neither the lift nor the notification sender.
static void _factoryPanelWaitMove(Task* task)
{
    ActionPrompt*     prompt;
    FactoryPanelWork* work;

    prompt              = D_80114D28;
    work                = task->work;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    if (work->moveSettled != 0) {
        if (gameFlagGetNibble(GAME_FLAG_FACTORY_POWER_ON) == 0) {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = FACTORY_PANEL_VIEW_UNPOWERED;
        } else {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = FACTORY_PANEL_VIEW_POWERED;
        }
        work->scanDelay   = FACTORY_PANEL_SCAN_DELAY_FRAMES;
        work->moveSettled = 0;
        task->state       = FACTORY_PANEL_STATE_IDLE;
    }
}
