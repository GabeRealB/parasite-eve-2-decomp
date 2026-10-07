/* Part of the factory lift library; see factory_lift.h. */

/// Runs the prompt state of the night factory script: hides the cursor and
/// stops it. If `itemMenuIsHotspotActionConfirmed` reports the Examine/Push
/// row was accepted, it runs the cap step `FactoryPanelWork::choice` selects.
/// Otherwise it advances to state 2. Both paths arm `scanDelay`.
void factoryPanelPrompt(Task* task)
{
    FactoryPanelWork* work = task->work;

    D_80114D28[0].mode        = ACTION_PROMPT_MODE_HIDDEN;
    D_80114D28[0].cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    if (itemMenuIsHotspotActionConfirmed() != 0) {
        factoryPanelRunStep(task, work->choice);
    } else {
        task->state = 2;
    }
    work->scanDelay = FACTORY_PANEL_SCAN_DELAY_FRAMES;
}
