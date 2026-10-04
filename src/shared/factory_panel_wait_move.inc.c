/* Part of the factory lift library; see factory_lift.h. */

/// Script state that waits for the lift to finish moving: keeps the prompt
/// hidden and, once `FactoryPanelWork::moveSettled` is raised, consumes it,
/// re-arms `scanDelay` and goes back to the idle state.
void factoryPanelWaitMove(Task* task)
{
    ActionPrompt*     prompt;
    FactoryPanelWork* work;

    prompt              = D_80114D28;
    work                = task->work;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    if (work->moveSettled != 0) {
        if (gameFlagGetNibble(GAME_FLAG_FACTORY_POWER_ON) == 0) {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0xC;
        } else {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 5;
        }
        work->scanDelay   = FACTORY_PANEL_SCAN_DELAY_FRAMES;
        work->moveSettled = 0;
        task->state       = 2;
    }
}
