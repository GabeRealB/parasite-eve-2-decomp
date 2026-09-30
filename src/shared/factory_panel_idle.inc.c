/* Part of the factory lift library; see factory_lift.h. */

/// Idle state of the room's script, state 2 of
/// `_gFactoryPanelStates`. It holds the prompt idle for the
/// `field_8` frames the prompt states armed -- decrementing that countdown
/// first and bailing out while it is still non-zero or while a cap is playing
/// -- and otherwise hit-tests the room's hotspot table.
///
/// A confirmed hit (`buttons[0].state == 2`) copies the hotspot's `id` and
/// `promptKind` into the work block and advances to state 3; with nothing under
/// the cursor the prompt merely highlights (`mode` 1). `buttons[1].state == 2`
/// leaves the scan by advancing to state 5.
void factoryPanelIdle(Task* task)
{
    RoomActionPrompt* prompt = D_80114D28;
    OverlayHotspot*   hs     = gFactoryPanelHotspots;
    FactoryPanelWork* st     = (FactoryPanelWork*)task->work;

    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (st->field_8 != 0) {
        st->field_8 = st->field_8 - 1;
    }
    if ((Gp_CapBusy() != 0) || (st->field_8 != 0)) {
        prompt->mode     = 0;
        prompt->targetId = 0;
        return;
    }
    prompt->targetId = 0x80;
    if (actionPromptHitTest(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = 2;
        if (prompt->buttons.slots[0].state == 2) {
            for (; hs->id != -1; hs++) {
                if (hs->hit != 0) {
                    prompt->mode     = 0;
                    prompt->targetId = 0;
                    st->field_C      = hs->id;
                    st->field_E      = hs->promptKind;
                    task->state      = 3;
                    return;
                }
            }
        }
    } else {
        prompt->mode = 1;
    }
    if (prompt->buttons.slots[1].state == 2) {
        task->state = 5;
    }
}
