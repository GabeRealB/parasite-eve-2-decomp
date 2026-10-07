/* Part of the toilet library; see toilet.h. */

/// Queues stage sound `0x52100005` when `arg2` is 5; otherwise does nothing.
s32 toiletSoundMsg(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 5) {
        sndEvtRequestStageScriptStart(0x52100000 | 5, 0, 0);
    }
    return 0;
}
