/* Part of the factory lift library; see factory_lift.h. */

/// Script message handler: raises the one-shot trigger the cursor state
/// consumes.
void factoryPanelTrigger(Task* task)
{
    ((FactoryPanelWork*)task->work)->field_A = 1;
}
