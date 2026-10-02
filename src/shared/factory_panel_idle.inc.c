/* Part of the factory lift library; see factory_lift.h. */

/// Idle state of the room's script, state 2 of
/// `_gFactoryPanelStates`. It holds the prompt idle for the
/// `field_8` frames the prompt states armed -- decrementing that countdown
/// first and bailing out while it is still non-zero or while a cap is playing
/// -- and otherwise hit-tests the room's hotspot table.
///
/// A confirmed hit (`buttons.slots[0].state == ACTION_PROMPT_BUTTON_PRESSED`)
/// copies the hotspot's `id` and `promptKind` into the work block and advances
/// to state 3; with nothing under the cursor the prompt keeps the idle cursor.
/// `buttons.slots[1].state == ACTION_PROMPT_BUTTON_PRESSED`
/// leaves the scan by advancing to state 5.
void factoryPanelIdle(Task* task)
{
    ActionPrompt*     prompt = D_80114D28;
    OverlayHotspot*   hs     = gFactoryPanelHotspots;
    FactoryPanelWork* st     = (FactoryPanelWork*)task->work;

    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (st->field_8 != 0) {
        st->field_8 = st->field_8 - 1;
    }
    if ((Gp_CapBusy() != 0) || (st->field_8 != 0)) {
        prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
        return;
    }
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    if (actionPromptHitTest(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HOTSPOT;
        if (prompt->buttons.slots[0].state == ACTION_PROMPT_BUTTON_PRESSED) {
            for (; hs->id != -1; hs++) {
                if (hs->hit != 0) {
                    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
                    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
                    st->field_C         = hs->id;
                    st->field_E         = hs->promptKind;
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
