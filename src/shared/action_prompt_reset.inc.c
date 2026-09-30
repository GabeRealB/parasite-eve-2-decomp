/* Part of the action prompt library; see action_prompt.h. */

/// State 0 of the prompt script task: resets both action-prompt slots - cursor
/// cleared, speed 0x100, double-press window 0xF frames, mode 1 - and steps the
/// task on one state.
void actionPromptReset(Task* task)
{
    RoomActionPrompt* prompt = D_80114D28;
    s32               i;

    for (i = 0; i < 2; i++, prompt++) {
        prompt->field_0                     = 0;
        prompt->field_4                     = 0;
        prompt->targetId                    = 0x100;
        prompt->field_E                     = 0xF;
        prompt->buttons.slots[0].heldFrames = 0;
        prompt->buttons.slots[1].heldFrames = 0;
        prompt->mode                        = 1;
    }
    task->state = task->state + 1;
}
