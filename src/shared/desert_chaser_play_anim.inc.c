/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Handler for message 0x7D3: latches the requested animation id into
/// `animId` and restarts the state machine at state 1.
s32 desertChaserMsgPlayAnim(Task* task, s32 arg1, AnimationPlayRequest* msg, s32 arg3)
{
    DesertChaserWork* work = task->work;

    work->animId    = msg->animationId;
    work->state     = 1;
    work->prevState = -1;
    return 0;
}
