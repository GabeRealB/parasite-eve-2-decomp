/* Part of the factory lift library; see factory_lift.h. */

/// Idle state of the room's script, state 2 of
/// `_gFactoryPanelStates`. It holds the prompt idle for the
/// `FactoryPanelWork::scanDelay` frames the prompt states armed --
/// decrementing that countdown first and bailing out while it is still
/// non-zero or while a cap is playing -- and otherwise hit-tests the room's
/// hotspot table.
///
/// A confirmed hit (`buttons.slots[0].state == ACTION_PROMPT_BUTTON_PRESSED`)
/// copies the hotspot's `id` and `promptKind` into the work block and advances
/// to state 3; with nothing under the cursor the prompt keeps the idle cursor.
/// `buttons.slots[1].state == ACTION_PROMPT_BUTTON_PRESSED`
/// leaves the scan by advancing to state 5.
void factoryPanelIdle(Task* task)
{
    ActionPrompt*        prompt = D_80114D28;
    ActionPromptHotspot* hs     = gFactoryPanelHotspots;
    FactoryPanelWork*    work   = task->work;

    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (work->scanDelay != 0) {
        work->scanDelay = work->scanDelay - 1;
    }
    if ((capIsBusy() != 0) || (work->scanDelay != 0)) {
        prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
        return;
    }
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    if (actionPromptHitTest(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HOTSPOT;
        if (prompt->buttons.slots[0].state == ACTION_PROMPT_BUTTON_PRESSED) {
            for (; hs->id != ACTION_PROMPT_HOTSPOT_END; hs++) {
                if (hs->hit != 0) {
                    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
                    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
                    work->choice        = hs->id;
                    work->promptKind    = hs->promptKind;
                    task->state         = 3;
                    return;
                }
            }
        }
    } else {
        prompt->mode = ACTION_PROMPT_MODE_IDLE;
    }
    if (prompt->buttons.slots[1].state == ACTION_PROMPT_BUTTON_PRESSED) {
        task->state = 5;
    }
}
