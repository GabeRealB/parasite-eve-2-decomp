/* Part of the factory lift library; see factory_lift.h. */

/// Runs the prompt state of the night factory script: hides the cursor and
/// stops it. While `func_800D4EC0` still reports a prompt on screen, it hands
/// the task to the cap step `FactoryPanelWork::choice` names. Once the prompt
/// is gone the task advances to state 2 instead, and either way the work
/// block's `scanDelay` is armed.
void factoryPanelPrompt(Task* task)
{
    FactoryPanelWork* work = task->work;

    D_80114D28[0].mode        = ACTION_PROMPT_MODE_HIDDEN;
    D_80114D28[0].cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    if (func_800D4EC0() != 0) {
        factoryPanelRunStep(task, work->choice);
    } else {
        task->state = 2;
    }
    work->scanDelay = FACTORY_PANEL_SCAN_DELAY_FRAMES;
}
