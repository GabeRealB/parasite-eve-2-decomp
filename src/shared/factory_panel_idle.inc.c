/* Part of the factory lift library; see factory_lift.h. */

/// Scans the operator panel's hotspots and latches a confirmed command choice.
///
/// Requires initialized panel work, a live prompt and a hotspot table ending
/// in `ACTION_PROMPT_HOTSPOT_END`. Hides the HUD and holds event input. The
/// scan delay counts down even while a CAP is busy; either wait hides and
/// stops the cursor. Confirm selects the first hit and opens its command
/// prompt; cancel leaves the panel. Cursor coordinates are screen pixels.
static void _factoryPanelIdle(Task* task)
{
    enum { FACTORY_PANEL_EVENT_ACTIVE = 1 };

    ActionPrompt*        prompt  = D_80114D28;
    ActionPromptHotspot* hotspot = gFactoryPanelHotspots;
    FactoryPanelWork*    work    = task->work;

    // Hold event input while captions and the post-choice delay settle.
    gGameSession->hideHud    = true;
    gGameSession->eventState = FACTORY_PANEL_EVENT_ACTIVE;
    if (work->scanDelay != 0) {
        work->scanDelay = work->scanDelay - 1;
    }
    if ((capIsBusy() != 0) || (work->scanDelay != 0)) {
        prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
        return;
    }
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    if (ACTION_PROMPT_HIT_TEST(hotspot, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HOTSPOT;
        if (prompt->buttons.slots[0].state == ACTION_PROMPT_BUTTON_PRESSED) {
            for (; hotspot->id != ACTION_PROMPT_HOTSPOT_END; hotspot++) {
                if (hotspot->hit != 0) {
                    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
                    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
                    work->choice        = hotspot->id;
                    work->promptKind    = hotspot->promptKind;
                    task->state         = FACTORY_PANEL_STATE_OPEN_PROMPT;
                    return;
                }
            }
        }
    } else {
        prompt->mode = ACTION_PROMPT_MODE_IDLE;
    }
    if (prompt->buttons.slots[1].state == ACTION_PROMPT_BUTTON_PRESSED) {
        task->state = FACTORY_PANEL_STATE_EXIT;
    }
}
