/* Part of the factory lift library; see factory_lift.h. */

/// Script state that waits for the one-shot trigger: keeps the prompt hidden
/// and, once `field_A` is raised, consumes it, re-arms the countdown and goes
/// back to the idle state.
void factoryPanelWaitMove(Task* task)
{
    RoomActionPrompt* prompt;
    FactoryPanelWork* work;

    prompt           = D_80114D28;
    work             = (FactoryPanelWork*)task->work;
    prompt->targetId = 0;
    prompt->mode     = 0;
    if (work->field_A != 0) {
        if (GameFlag_GetNibble(0x48) == 0) {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0xC;
        } else {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 5;
        }
        work->field_8 = 0xA;
        work->field_A = 0;
        task->state   = 2;
    }
}
