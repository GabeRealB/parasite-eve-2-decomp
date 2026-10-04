/* Part of the Actor messages library; see actor_messages.h. */

/// Leaves state 0xD: to 0xE while the player has HP left, to 0x16 once it is
/// gone. Any other state is left alone.
s32 actorMsgReleaseHold(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    ActorMsgStateWork* work = task->work;
    PlayerStatus*      cfg  = &gPlayerStatus;

    if (work->state == ACTOR_MESSAGE_STATE_GRAB_HOLD) {
        if (cfg->hp > 0) {
            work->state = ACTOR_MESSAGE_STATE_GRAB_DONE;
        } else {
            work->state = ACTOR_MESSAGE_STATE_DORMANT;
        }
    }
    return 1;
}
