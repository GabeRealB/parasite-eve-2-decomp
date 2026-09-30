/* Part of the factory lift library; see factory_lift.h. */

/// Script state: drops the prompt's highlight and spawns the action prompt at
/// the cursor position with the display mode of the confirmed hotspot, then
/// moves the script to state 4.
void factoryPanelOpenPrompt(Task* task)
{
    RoomActionPrompt* prompt = D_80114D28;
    FactoryPanelWork* work   = (FactoryPanelWork*)task->work;

    prompt->mode     = 0;
    prompt->targetId = 0;
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, work->field_E);
    task->state = 4;
}
