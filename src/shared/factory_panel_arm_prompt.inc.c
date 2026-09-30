/* Part of the factory lift library; see factory_lift.h. */

/// Script state: highlights the action prompt (`mode` 1, target id 0x80),
/// clears its screen position and steps the script on one state.
void factoryPanelArmPrompt(Task* task)
{
    RoomActionPrompt* prompt = D_80114D28;

    prompt->targetId    = 0x80;
    prompt->mode        = 1;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}
