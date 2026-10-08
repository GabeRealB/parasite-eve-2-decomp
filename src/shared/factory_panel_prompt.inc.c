/* Part of the factory lift library; see factory_lift.h. */

/// Applies an accepted operator-panel choice or returns to hotspot scanning.
///
/// Requires initialized panel work with a latched hotspot choice and a completed
/// command prompt. Hides/stops the cursor before testing acceptance; accepted
/// choices select dialogue or lift motion, while cancellation returns to IDLE.
/// Both paths arm the ten-frame scan delay. Works in both factory variants.
static void _factoryPanelPrompt(Task* task)
{
    FactoryPanelWork* work = task->work;

    D_80114D28[0].mode        = ACTION_PROMPT_MODE_HIDDEN;
    D_80114D28[0].cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    if (itemMenuIsHotspotActionConfirmed() != 0) {
        _factoryPanelApplyChoice(task, work->choice);
    } else {
        task->state = FACTORY_PANEL_STATE_IDLE;
    }
    work->scanDelay = FACTORY_PANEL_SCAN_DELAY_FRAMES;
}
