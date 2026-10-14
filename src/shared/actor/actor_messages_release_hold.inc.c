/* Part of the Actor messages library; see actor_messages.h. */

s32 actorMsgReleaseHold(Task* task, s32 msgId, s32 unusedFirstArg, s32 unusedArg)
{
    ActorMsgStateWork*  work         = task->work;
    const PlayerStatus* playerStatus = &gPlayerStatus;

    if (work->state == ACTOR_MESSAGE_STATE_GRAB_HOLD) {
        if (playerStatus->hp > 0) {
            work->state = ACTOR_MESSAGE_STATE_GRAB_DONE;
        } else {
            work->state = ACTOR_MESSAGE_STATE_DORMANT;
        }
    }
    return 1;
}
