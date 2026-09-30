/* Part of the factory lift library; see factory_lift.h. */

/// Runs the prompt state of the night factory script: drops the highlight the
/// previous state left in `D_80114D28` and, while `func_800D4EC0` still reports
/// a prompt on screen, hands the task to the cap step `field_C` names. Once the
/// prompt is gone the task advances to state 2 instead, and either way the work
/// block's `field_8` is set to 0xA.
void factoryPanelPrompt(Task* task)
{
    FactoryPanelWork* work = (FactoryPanelWork*)task->work;

    D_80114D28[0].mode     = 0;
    D_80114D28[0].targetId = 0;
    if (func_800D4EC0() != 0) {
        factoryPanelRunStep(task, work->field_C);
    } else {
        task->state = 2;
    }
    work->field_8 = 0xA;
}
