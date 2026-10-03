/* Part of the factory lift library; see factory_lift.h. */

/// Script message handler: raises `FactoryPanelWork::moveSettled`, which the
/// state waiting for the lift consumes.
void factoryPanelTrigger(Task* task)
{
    FactoryPanelWork* work = task->work;

    work->moveSettled = 1;
}
