/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Handler for message 0x7D3: latches the requested animation id into
/// `field_82E` and restarts the state machine at state 1.
s32 desertChaserMsgPlayAnim(Task* task, s32 arg1, AnimationPlayRequest* msg, s32 arg3)
{
    DesertChaserWork* work = (DesertChaserWork*)task->work;

    work->field_82E = msg->animationId;
    work->field_0   = 1;
    work->field_2   = -1;
    return 0;
}
