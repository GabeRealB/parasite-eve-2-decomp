/* Part of the general store library; see general_store.h. */

/// Message handler that plays stage sound 0x52030007 on action 7. Always
/// returns 0.
s32 storeSoundMsg(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 7) {
        sndEvtRequestStageScriptStart(0x52030000 | 7, 0, 0);
    }
    return 0;
}
