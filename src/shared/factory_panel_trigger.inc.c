/* Part of the factory lift library; see factory_lift.h. */

/// Script message handler: raises `FactoryPanelWork::moveSettled`, which the
/// state waiting for the lift consumes.
void factoryPanelTrigger(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    FactoryPanelWork* work = task->work;

    work->moveSettled = 1;
}
